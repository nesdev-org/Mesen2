#include "pch.h"
#include "NES/EnhancedSynth.h"
#include "NES/NesConsole.h"
#include "NES/APU/NesApu.h"
#include "Shared/Emulator.h"
#include "Shared/Audio/SoundMixer.h"

EnhancedSynth::EnhancedSynth(Emulator* emu, NesConsole* console)
{
	_emu = emu;
	_console = console;
	_emu->GetSoundMixer()->RegisterAudioProvider(this);
}

EnhancedSynth::~EnhancedSynth()
{
	_emu->GetSoundMixer()->UnregisterAudioProvider(this);
}

double EnhancedSynth::PolyBlep(double t, double dt)
{
	//Removes most of the aliasing from naive saw/pulse edges
	if(t < dt) {
		t /= dt;
		return t + t - t * t - 1.0;
	} else if(t > 1.0 - dt) {
		t = (t - 1.0) / dt;
		return t * t + t + t + 1.0;
	}
	return 0.0;
}

double EnhancedSynth::BlepSaw(double phase, double inc)
{
	return (2.0 * phase - 1.0) - PolyBlep(phase, inc);
}

void EnhancedSynth::Retrigger(Voice& voice, double freq, double vol)
{
	//The NES restarts the sequencer on $4003/$400B writes. The synth can't see
	//the write itself (state is polled), so a new note is inferred from a
	//volume rise out of silence or a pitch step larger than vibrato depth.
	//3% pitch threshold (~half a semitone): per-frame slides/vibrato steps stay
	//below it (Shadow Man's intro slide is ~1.6%/frame), a semitone (~6%) fires.
	//If deep vibrato ever clicks in-game, raise to 6-8% or drop the pitch trigger.
	bool volAttack = vol > 0.001 && voice.LastVol <= 0.001;
	bool pitchJump = vol > 0.001 && voice.LastFreq > 0 && freq > 0 && std::abs(freq - voice.LastFreq) / voice.LastFreq > 0.03;
	if(volAttack || pitchJump) {
		voice.Phase = 0;
		voice.PhaseB = 0;
		voice.SubPhase = 0;
		//Dip the smoothed volume so the attack ramp is re-articulated
		voice.SmoothedVol *= 0.25;
	}
	voice.LastFreq = freq;
	voice.LastVol = vol;
}

double EnhancedSynth::NextNoise()
{
	//xorshift32, mapped to -1..1
	_noiseRng ^= _noiseRng << 13;
	_noiseRng ^= _noiseRng >> 17;
	_noiseRng ^= _noiseRng << 5;
	return (int32_t)_noiseRng / 2147483648.0;
}

void EnhancedSynth::MixAudio(int16_t* out, uint32_t sampleCount, uint32_t sampleRate)
{
	NesConfig& cfg = _console->GetNesConfig();
	if(!cfg.EnableEnhancedAudio || _emu->IsRunAheadFrame() || sampleRate == 0) {
		return;
	}

	//M0 limitation: called once per APU flush (~5.6ms), so slides/vibrato that
	//sound drivers rewrite every PPU frame are stair-stepped at this rate.
	//M1 should sample the APU state at least once per PPU frame (see PRD FR1.1).
	ApuState apu = _console->GetApu()->GetState();

	auto envVolume = [](ApuEnvelopeState& env) {
		return (env.ConstantVolume ? env.Volume : env.Counter) / 15.0;
	};

	double leadVol = 0, leadFreq = apu.Square1.Frequency;
	if(apu.Square1.Enabled && apu.Square1.LengthCounter.Counter > 0 && apu.Square1.Period >= 8) {
		leadVol = envVolume(apu.Square1.Envelope);
	}
	double harmVol = 0, harmFreq = apu.Square2.Frequency;
	if(apu.Square2.Enabled && apu.Square2.LengthCounter.Counter > 0 && apu.Square2.Period >= 8) {
		harmVol = envVolume(apu.Square2.Envelope);
	}
	double bassVol = 0, bassFreq = apu.Triangle.Frequency;
	if(apu.Triangle.Enabled && apu.Triangle.LengthCounter.Counter > 0 && apu.Triangle.LinearCounter > 0 && apu.Triangle.Period >= 2) {
		bassVol = 1.0;
	}
	double noiseVol = 0;
	if(apu.Noise.Enabled && apu.Noise.LengthCounter.Counter > 0) {
		noiseVol = envVolume(apu.Noise.Envelope);
	}

	Retrigger(_lead, leadFreq, leadVol);
	Retrigger(_harmony, harmFreq, harmVol);
	Retrigger(_bass, bassFreq, bassVol);

	//The duty cycle is part of the arrangement (12.5% leads vs 50% pads);
	//map it to the synth's pulse width so that character survives.
	//Duty 3 (75%) sounds identical to 25% on the NES, so it maps back to 0.25.
	static constexpr double dutyWidth[4] = { 0.125, 0.25, 0.5, 0.25 };
	double leadWidth = dutyWidth[apu.Square1.Duty & 0x03];
	double harmWidth = dutyWidth[apu.Square2.Duty & 0x03];

	//Map the noise shift rate to timbre brightness (fast LFSR = hi-hat, slow = snare/tom body)
	double noiseBrightness = std::min(1.0, apu.Noise.Frequency / 200000.0);

	//~4ms one-pole volume smoothing removes zipper noise between flushes
	double volSmooth = 1.0 - std::exp(-1.0 / (sampleRate * 0.004));
	double masterGain = (cfg.EnhancedAudioVolume / 100.0) * 5500.0;

	double leadInc = leadFreq / sampleRate;
	double harmInc = harmFreq / sampleRate;
	double bassInc = bassFreq / sampleRate;
	double leadLpCoeff = 1.0 - std::exp(-2.0 * 3.14159265 * 5200.0 / sampleRate);
	double harmLpCoeff = 1.0 - std::exp(-2.0 * 3.14159265 * 3000.0 / sampleRate);
	double bassLpCoeff = 1.0 - std::exp(-2.0 * 3.14159265 * 800.0 / sampleRate);
	double noiseLpCoeff = 1.0 - std::exp(-2.0 * 3.14159265 * 2800.0 / sampleRate);
	double noiseHpCoeff = 1.0 - std::exp(-2.0 * 3.14159265 * 6500.0 / sampleRate);

	auto step = [](double& phase, double inc) {
		phase += inc;
		if(phase >= 1.0) {
			phase -= 1.0;
		}
		return phase;
	};
	//Pulse with variable width out of two anti-aliased saws (saw(t) - saw(t+width))
	auto pulse = [&](double phase, double inc, double width) {
		double shifted = phase + width;
		if(shifted >= 1.0) {
			shifted -= 1.0;
		}
		return BlepSaw(phase, inc) - BlepSaw(shifted, inc);
	};

	for(uint32_t i = 0; i < sampleCount; i++) {
		_lead.SmoothedVol += (leadVol - _lead.SmoothedVol) * volSmooth;
		_harmony.SmoothedVol += (harmVol - _harmony.SmoothedVol) * volSmooth;
		_bass.SmoothedVol += (bassVol - _bass.SmoothedVol) * volSmooth;
		_noiseVol += (noiseVol - _noiseVol) * volSmooth;

		//Lead: detuned pulse pair (width follows the duty) + saw body
		double lead = 0.45 * pulse(step(_lead.Phase, leadInc * 1.004), leadInc, leadWidth)
			+ 0.45 * pulse(step(_lead.PhaseB, leadInc * 0.996), leadInc, leadWidth)
			+ 0.25 * BlepSaw(step(_lead.SubPhase, leadInc), leadInc);
		_lead.Lp += (lead - _lead.Lp) * leadLpCoeff;
		lead = _lead.Lp * _lead.SmoothedVol;

		//Harmony: softer single detuned pulse pair
		double harm = 0.45 * pulse(step(_harmony.Phase, harmInc * 1.002), harmInc, harmWidth)
			+ 0.45 * pulse(step(_harmony.PhaseB, harmInc * 0.998), harmInc, harmWidth);
		_harmony.Lp += (harm - _harmony.Lp) * harmLpCoeff;
		harm = _harmony.Lp * _harmony.SmoothedVol * 0.7;

		//Bass: sine + saw + half-frequency sub sine
		step(_bass.Phase, bassInc);
		step(_bass.SubPhase, bassInc * 0.5);
		double bass = 0.65 * std::sin(2.0 * 3.14159265 * _bass.Phase)
			+ 0.4 * BlepSaw(step(_bass.PhaseB, bassInc), bassInc)
			+ 0.3 * std::sin(2.0 * 3.14159265 * _bass.SubPhase);
		_bass.Lp += (bass - _bass.Lp) * bassLpCoeff;
		bass = _bass.Lp * _bass.SmoothedVol;

		//Drums: noise split into a dark body and a bright top, blended by LFSR rate
		double n = NextNoise();
		_noiseLp += (n - _noiseLp) * noiseLpCoeff;
		_noiseHpState += (n - _noiseHpState) * noiseHpCoeff;
		double drum = ((1.0 - noiseBrightness) * _noiseLp * 1.6 + noiseBrightness * (n - _noiseHpState)) * _noiseVol;

		double sample = lead * 1.0 + harm * 0.8 + bass * 0.95 + drum * 0.9;
		//Cheap soft clip keeps the sum inside int16 headroom
		sample = sample / (1.0 + std::abs(sample) * 0.35) * masterGain;

		int32_t left = (int32_t)out[i * 2] + (int32_t)sample;
		int32_t right = (int32_t)out[i * 2 + 1] + (int32_t)sample;
		out[i * 2] = (int16_t)std::clamp(left, -32768, 32767);
		out[i * 2 + 1] = (int16_t)std::clamp(right, -32768, 32767);
	}
}
