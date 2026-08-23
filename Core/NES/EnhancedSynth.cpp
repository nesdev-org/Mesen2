#include "pch.h"
#include "NES/EnhancedSynth.h"
#include "NES/NesConsole.h"
#include "NES/APU/NesApu.h"
#include "Shared/Emulator.h"
#include "Shared/Audio/SoundMixer.h"

//Built-in instrument presets. Order must match the EnhancedAudioPreset enum
//on the UI side (Synthwave = 0, ChipDeluxe = 1, OrchestralLite = 2, Dry = 3).
static constexpr EnhancedSynthPreset _presets[4] = {
	//Synthwave: detuned pulse-width leads, saw+sub bass, tight drums
	{
		0.004, 0.002, true, 0.5, 0.25, 5200, 3200, 1.4,
		0.65, 0.45, 0.35, 900, 1.3,
		1400, 6800, 6500, 1.2, 0.5, 0.06, 165,
		4, 4,
		0.24, 0.45, 0.65, 0.16,
		1.0, 0.56, 0.85, 0.75
	},
	//Chip deluxe: stays close to the 2A03 character - pure-ish pulses,
	//round bass, crisp drums, just a touch of space
	{
		0.0015, 0.001, true, 0.5, 0.10, 8000, 6000, 1.1,
		0.9, 0.1, 0.2, 1200, 1.0,
		1800, 7500, 7000, 1.0, 0.35, 0.045, 165,
		2, 3,
		0.12, 0.25, 0.35, 0.08,
		1.0, 0.6, 0.8, 0.85
	},
	//Orchestral lite: slow-attack string-like leads, low string bass,
	//timpani-weight drums, larger room
	{
		0.007, 0.005, false, 0.5, 0.35, 3800, 2600, 1.0,
		0.5, 0.6, 0.25, 700, 1.2,
		900, 4500, 6000, 1.0, 0.7, 0.12, 110,
		35, 80,
		0.30, 0.30, 0.45, 0.30,
		0.95, 0.65, 0.9, 0.6
	},
	//Dry: Synthwave voices with no echo/reverb tail - SFX stay tight
	{
		0.004, 0.002, true, 0.5, 0.25, 5200, 3200, 1.4,
		0.65, 0.45, 0.35, 900, 1.3,
		1400, 6800, 6500, 1.2, 0.5, 0.06, 165,
		3, 4,
		0.05, 0.0, 0.0, 0.0,
		1.0, 0.56, 0.85, 0.75
	},
};

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

const EnhancedSynthPreset& EnhancedSynth::GetPreset(uint32_t presetId)
{
	return _presets[presetId < 4 ? presetId : 0];
}

void EnhancedSynth::Reset()
{
	std::fill(_echoBuf.begin(), _echoBuf.end(), 0.0f);
	std::fill(_revBufL.begin(), _revBufL.end(), 0.0f);
	std::fill(_revBufR.begin(), _revBufR.end(), 0.0f);
	_lead = {};
	_harmony = {};
	_bass = {};
	_noiseVol = 0;
	_drumLpLow = _drumLpHigh = _drumLpTop = 0;
	_thumpGate = 0;
	_thumpPhase = 0;
	_lastNoisePollVol = 0;
	_wasActive = false;
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
	//Reset oscillator phases only on an attack out of silence - the voice is
	//near-silent there, so the reset is inaudible. Legato pitch changes keep
	//the phase continuous: resetting mid-note produces a waveform
	//discontinuity (a click per note), which on fast melodic lines turns
	//into rhythmic crackle.
	bool volAttack = vol > 0.001 && voice.LastVol <= 0.001;
	if(volAttack) {
		voice.Phase = 0;
		voice.PhaseB = 0;
		voice.SubPhase = 0;
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
			//would replay stale audio frozen in the buffers
			Reset();
		}
		return;
	}
	_wasActive = true;

	const EnhancedSynthPreset& p = GetPreset(cfg.EnhancedAudioPreset);

	//The synth state is polled once per audio flush (~5.6ms / ~179Hz), which is
	//~3x the 60Hz rate NES sound drivers update their registers at - so control
	//changes land with at most one flush (~5.6ms) of latency, no stair-stepping
	//beyond what the driver itself produces.
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
	double leadWidth = p.FollowDuty ? dutyWidth[apu.Square1.Duty & 0x03] : p.FixedWidth;
	double harmWidth = p.FollowDuty ? dutyWidth[apu.Square2.Duty & 0x03] : p.FixedWidth;

	//Map the noise shift rate to drum timbre: fast LFSR = bright hi-hat top,
	//slow = snare/tom body. A low thump is triggered only on attacks (volume
	//rising into a slow+loud noise), so sustained noise (wind, engines) does
	//not turn into a hum - the thump then decays on its own.
	double noiseBrightness = std::min(1.0, apu.Noise.Frequency / 200000.0);
	if(apu.Noise.Frequency <= 15000.0 && noiseVol >= 0.65 && noiseVol > _lastNoisePollVol + 0.08) {
		_thumpGate = 1.0;
		_thumpPhase = 0;
	}
	_lastNoisePollVol = noiseVol;
	//Time constants are clamped so a future user-editable preset (JSON) can't
	//produce division by zero or denormal-slow smoothing
	double thumpDecay = std::exp(-1.0 / (sampleRate * std::max(0.005, p.ThumpDecayS)));

	//Separate attack/release smoothing (orchestral preset uses slow attacks)
	double attackCoeff = 1.0 - std::exp(-1.0 / (sampleRate * std::max(0.5, p.AttackMs) / 1000.0));
	double releaseCoeff = 1.0 - std::exp(-1.0 / (sampleRate * std::max(0.5, p.ReleaseMs) / 1000.0));
	double masterGain = (cfg.EnhancedAudioVolume / 100.0) * 5000.0;

	double leadInc = leadFreq / sampleRate;
	double harmInc = harmFreq / sampleRate;
	double bassInc = bassFreq / sampleRate;
	double leadLpCoeff = 1.0 - std::exp(-pi2 * p.LeadLpHz / sampleRate);
	double harmLpCoeff = 1.0 - std::exp(-pi2 * p.HarmLpHz / sampleRate);
	double bassLpCoeff = 1.0 - std::exp(-pi2 * p.BassLpHz / sampleRate);
	double drumLowCoeff = 1.0 - std::exp(-pi2 * p.DrumBodyLoHz / sampleRate);
	double drumHighCoeff = 1.0 - std::exp(-pi2 * p.DrumBodyHiHz / sampleRate);
	double drumTopCoeff = 1.0 - std::exp(-pi2 * p.DrumTopHz / sampleRate);
	double thumpInc = p.ThumpFreqHz / sampleRate;

	//Delay lines: lead echo + 83/127/173ms feedforward reverb taps
	uint32_t echoSize = (uint32_t)_echoBuf.size();
	uint32_t revSize = (uint32_t)_revBufL.size();
	uint32_t echoDelay = std::clamp((uint32_t)(p.EchoDelayS * sampleRate), 1u, echoSize - 1);
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
	auto smooth = [&](double& state, double target) {
		state += (target - state) * (target > state ? attackCoeff : releaseCoeff);
	};

	for(uint32_t i = 0; i < sampleCount; i++) {
		smooth(_lead.SmoothedVol, leadVol);
		smooth(_harmony.SmoothedVol, harmVol);
		smooth(_bass.SmoothedVol, bassVol);
		smooth(_noiseVol, noiseVol);
		_thumpGate *= thumpDecay;

		//Lead: detuned pulse pair + octave-up saw shimmer
		double lead = 0.45 * pulse(step(_lead.Phase, leadInc * (1.0 + p.LeadDetune)), leadInc * (1.0 + p.LeadDetune), leadWidth)
			+ 0.45 * pulse(step(_lead.PhaseB, leadInc * (1.0 - p.LeadDetune)), leadInc * (1.0 - p.LeadDetune), leadWidth)
			+ p.LeadOctaveUpMix * BlepSaw(step(_lead.SubPhase, leadInc * 2.0), leadInc * 2.0);
		lead = softClip(lead * p.LeadDrive);
		_lead.Lp += (lead - _lead.Lp) * leadLpCoeff;
		lead = _lead.Lp * _lead.SmoothedVol;

		//Harmony: softer detuned pulse pair
		double harm = 0.45 * pulse(step(_harmony.Phase, harmInc * (1.0 + p.HarmDetune)), harmInc * (1.0 + p.HarmDetune), harmWidth)
			+ 0.45 * pulse(step(_harmony.PhaseB, harmInc * (1.0 - p.HarmDetune)), harmInc * (1.0 - p.HarmDetune), harmWidth);
		_harmony.Lp += (harm - _harmony.Lp) * harmLpCoeff;
		harm = _harmony.Lp * _harmony.SmoothedVol;

		//Bass: sine + saw + half-frequency sub sine, mildly driven
		step(_bass.Phase, bassInc);
		step(_bass.SubPhase, bassInc * 0.5);
		double bass = p.BassSine * std::sin(pi2 * _bass.Phase)
			+ p.BassSaw * BlepSaw(step(_bass.PhaseB, bassInc), bassInc)
			+ p.BassSub * std::sin(pi2 * _bass.SubPhase);
		bass = softClip(bass * p.BassDrive);
		_bass.Lp += (bass - _bass.Lp) * bassLpCoeff;
		bass = _bass.Lp * _bass.SmoothedVol;

		//Drums: bandpassed body vs highpassed top blended by LFSR rate + thump
		double n = NextNoise();
		_drumLpLow += (n - _drumLpLow) * drumLowCoeff;
		_drumLpHigh += (n - _drumLpHigh) * drumHighCoeff;
		_drumLpTop += (n - _drumLpTop) * drumTopCoeff;
		double body = (_drumLpHigh - _drumLpLow) * p.DrumBodyGain;
		double top = n - _drumLpTop;
		step(_thumpPhase, thumpInc);
		double drum = (noiseBrightness * top + (1.0 - noiseBrightness) * body) * _noiseVol
			+ p.ThumpGain * std::sin(pi2 * _thumpPhase) * _thumpGate * _noiseVol;

		//Lead echo (single tap)
		uint32_t echoRead = (_echoPos + echoSize - echoDelay) % echoSize;
		double echo = _echoBuf[echoRead];
		_echoBuf[_echoPos] = (float)lead;
		_echoPos = (_echoPos + 1) % echoSize;

		//Stereo image: harmony left, lead echo right
		double left = p.LeadGain * lead + p.EchoGainL * echo + 1.25 * p.HarmGain * harm + p.BassGain * bass + p.DrumGain * drum;
		double right = p.LeadGain * lead + p.EchoGainR * echo + 0.80 * p.HarmGain * harm + p.BassGain * bass + p.DrumGain * drum;

		//Light feedforward reverb (3 taps)
		_revBufL[_revPos] = (float)left;
		_revBufR[_revPos] = (float)right;
		uint32_t t1 = (_revPos + revSize - revTap1) % revSize;
		uint32_t t2 = (_revPos + revSize - revTap2) % revSize;
		uint32_t t3 = (_revPos + revSize - revTap3) % revSize;
		left += p.ReverbWet * (0.35 * _revBufL[t1] + 0.28 * _revBufL[t2] + 0.22 * _revBufL[t3]);
		right += p.ReverbWet * (0.35 * _revBufR[t1] + 0.28 * _revBufR[t2] + 0.22 * _revBufR[t3]);
		_revPos = (_revPos + 1) % revSize;

		int32_t outL = (int32_t)out[i * 2] + (int32_t)(softClip(left) * masterGain);
		int32_t outR = (int32_t)out[i * 2 + 1] + (int32_t)(softClip(right) * masterGain);
		out[i * 2] = (int16_t)std::clamp(outL, -32768, 32767);
		out[i * 2 + 1] = (int16_t)std::clamp(outR, -32768, 32767);
	}
}
