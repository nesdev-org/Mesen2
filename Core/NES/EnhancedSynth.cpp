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
	constexpr double pi2 = 2.0 * 3.14159265358979;

	NesConfig& cfg = _console->GetNesConfig();
	if(!cfg.EnableEnhancedAudio || _emu->IsRunAheadFrame() || sampleRate == 0) {
		if(_wasActive && !cfg.EnableEnhancedAudio) {
			//Clear delay lines and voice state, otherwise re-enabling the synth
			//would replay ~240ms of stale audio frozen in the buffers
			std::fill(_echoBuf.begin(), _echoBuf.end(), 0.0f);
			std::fill(_revBufL.begin(), _revBufL.end(), 0.0f);
			std::fill(_revBufR.begin(), _revBufR.end(), 0.0f);
			_lead = {};
			_harmony = {};
			_bass = {};
			_noiseVol = 0;
			_drumLpLow = _drumLpHigh = _drumLpTop = 0;
			_thumpGate = 0;
			_lastNoisePollVol = 0;
			_wasActive = false;
		}
		return;
	}
	_wasActive = true;

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

	//Map the noise shift rate to drum timbre: fast LFSR = bright hi-hat top,
	//slow = snare/tom body. A low thump is triggered only on attacks (volume
	//rising into a slow+loud noise), so sustained noise (wind, engines) does
	//not turn into a 165Hz hum - the thump then decays on its own.
	double noiseBrightness = std::min(1.0, apu.Noise.Frequency / 200000.0);
	if(apu.Noise.Frequency <= 15000.0 && noiseVol >= 0.65 && noiseVol > _lastNoisePollVol + 0.08) {
		_thumpGate = 1.0;
		_thumpPhase = 0;
	}
	_lastNoisePollVol = noiseVol;
	double thumpDecay = std::exp(-1.0 / (sampleRate * 0.06));

	//~4ms one-pole volume smoothing removes zipper noise between flushes
	double volSmooth = 1.0 - std::exp(-1.0 / (sampleRate * 0.004));
	double masterGain = (cfg.EnhancedAudioVolume / 100.0) * 5000.0;

	double leadInc = leadFreq / sampleRate;
	double harmInc = harmFreq / sampleRate;
	double bassInc = bassFreq / sampleRate;
	double leadLpCoeff = 1.0 - std::exp(-pi2 * 5200.0 / sampleRate);
	double harmLpCoeff = 1.0 - std::exp(-pi2 * 3200.0 / sampleRate);
	double bassLpCoeff = 1.0 - std::exp(-pi2 * 900.0 / sampleRate);
	double drumLowCoeff = 1.0 - std::exp(-pi2 * 1400.0 / sampleRate);
	double drumHighCoeff = 1.0 - std::exp(-pi2 * 6800.0 / sampleRate);
	double drumTopCoeff = 1.0 - std::exp(-pi2 * 6500.0 / sampleRate);
	double thumpInc = 165.0 / sampleRate;

	//Delay lines: 240ms lead echo, 83/127/173ms feedforward reverb taps
	uint32_t echoSize = (uint32_t)_echoBuf.size();
	uint32_t revSize = (uint32_t)_revBufL.size();
	uint32_t echoDelay = std::min(echoSize - 1, (uint32_t)(0.24 * sampleRate));
	uint32_t revTap1 = std::min(revSize - 1, (uint32_t)(0.083 * sampleRate));
	uint32_t revTap2 = std::min(revSize - 1, (uint32_t)(0.127 * sampleRate));
	uint32_t revTap3 = std::min(revSize - 1, (uint32_t)(0.173 * sampleRate));

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
	auto softClip = [](double x) {
		return x / (1.0 + std::abs(x) * 0.35);
	};

	for(uint32_t i = 0; i < sampleCount; i++) {
		_lead.SmoothedVol += (leadVol - _lead.SmoothedVol) * volSmooth;
		_harmony.SmoothedVol += (harmVol - _harmony.SmoothedVol) * volSmooth;
		_bass.SmoothedVol += (bassVol - _bass.SmoothedVol) * volSmooth;
		_noiseVol += (noiseVol - _noiseVol) * volSmooth;
		_thumpGate *= thumpDecay;

		//Lead: detuned pulse pair (width follows the duty) + octave-up saw shimmer
		double lead = 0.45 * pulse(step(_lead.Phase, leadInc * 1.004), leadInc * 1.004, leadWidth)
			+ 0.45 * pulse(step(_lead.PhaseB, leadInc * 0.996), leadInc * 0.996, leadWidth)
			+ 0.25 * BlepSaw(step(_lead.SubPhase, leadInc * 2.0), leadInc * 2.0);
		lead = softClip(lead * 1.4);
		_lead.Lp += (lead - _lead.Lp) * leadLpCoeff;
		lead = _lead.Lp * _lead.SmoothedVol;

		//Harmony: softer single detuned pulse pair
		double harm = 0.45 * pulse(step(_harmony.Phase, harmInc * 1.002), harmInc * 1.002, harmWidth)
			+ 0.45 * pulse(step(_harmony.PhaseB, harmInc * 0.998), harmInc * 0.998, harmWidth);
		_harmony.Lp += (harm - _harmony.Lp) * harmLpCoeff;
		harm = _harmony.Lp * _harmony.SmoothedVol;

		//Bass: sine + saw + half-frequency sub sine, mildly driven
		step(_bass.Phase, bassInc);
		step(_bass.SubPhase, bassInc * 0.5);
		double bass = 0.65 * std::sin(pi2 * _bass.Phase)
			+ 0.45 * BlepSaw(step(_bass.PhaseB, bassInc), bassInc)
			+ 0.35 * std::sin(pi2 * _bass.SubPhase);
		bass = softClip(bass * 1.3);
		_bass.Lp += (bass - _bass.Lp) * bassLpCoeff;
		bass = _bass.Lp * _bass.SmoothedVol;

		//Drums: 1.4-6.8kHz band as snare/tom body, highpassed top as hi-hat,
		//blended by LFSR rate, plus a 165Hz thump on slow+loud noise (kick/snare weight)
		double n = NextNoise();
		_drumLpLow += (n - _drumLpLow) * drumLowCoeff;
		_drumLpHigh += (n - _drumLpHigh) * drumHighCoeff;
		_drumLpTop += (n - _drumLpTop) * drumTopCoeff;
		double body = (_drumLpHigh - _drumLpLow) * 1.2;
		double top = n - _drumLpTop;
		step(_thumpPhase, thumpInc);
		double drum = (noiseBrightness * top + (1.0 - noiseBrightness) * body) * _noiseVol
			+ 0.5 * std::sin(pi2 * _thumpPhase) * _thumpGate * _noiseVol;

		//Lead echo (single 240ms tap)
		uint32_t echoRead = (_echoPos + echoSize - echoDelay) % echoSize;
		double echo = _echoBuf[echoRead];
		_echoBuf[_echoPos] = (float)lead;
		_echoPos = (_echoPos + 1) % echoSize;

		//Stereo image from the offline remaster: harmony left, echo right
		double left = lead + 0.45 * echo + 0.70 * harm + 0.85 * bass + 0.75 * drum;
		double right = lead + 0.65 * echo + 0.45 * harm + 0.85 * bass + 0.75 * drum;

		//Light feedforward reverb (3 taps)
		_revBufL[_revPos] = (float)left;
		_revBufR[_revPos] = (float)right;
		uint32_t t1 = (_revPos + revSize - revTap1) % revSize;
		uint32_t t2 = (_revPos + revSize - revTap2) % revSize;
		uint32_t t3 = (_revPos + revSize - revTap3) % revSize;
		left += 0.16 * (0.35 * _revBufL[t1] + 0.28 * _revBufL[t2] + 0.22 * _revBufL[t3]);
		right += 0.16 * (0.35 * _revBufR[t1] + 0.28 * _revBufR[t2] + 0.22 * _revBufR[t3]);
		_revPos = (_revPos + 1) % revSize;

		int32_t outL = (int32_t)out[i * 2] + (int32_t)(softClip(left) * masterGain);
		int32_t outR = (int32_t)out[i * 2 + 1] + (int32_t)(softClip(right) * masterGain);
		out[i * 2] = (int16_t)std::clamp(outL, -32768, 32767);
		out[i * 2 + 1] = (int16_t)std::clamp(outR, -32768, 32767);
	}
}
