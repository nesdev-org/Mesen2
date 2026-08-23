#pragma once
#include "pch.h"
#include "Shared/Interfaces/IAudioProvider.h"

class Emulator;
class NesConsole;

//Instrument definition for the enhanced audio synth. Three built-in presets
//are defined in EnhancedSynth.cpp; a user-editable JSON layer can be added
//on top of this later without changing the synth itself.
struct EnhancedSynthPreset
{
	//Pulse voices
	double LeadDetune;
	double HarmDetune;
	bool FollowDuty;        //pulse width follows the game's duty setting
	double FixedWidth;      //used when FollowDuty is false
	double LeadOctaveUpMix;
	double LeadLpHz;
	double HarmLpHz;
	double LeadDrive;

	//Bass (triangle)
	double BassSine;
	double BassSaw;
	double BassSub;
	double BassLpHz;
	double BassDrive;

	//Drums (noise)
	double DrumBodyLoHz;
	double DrumBodyHiHz;
	double DrumTopHz;
	double DrumBodyGain;
	double ThumpGain;
	double ThumpDecayS;
	double ThumpFreqHz;

	//Envelope smoothing
	double AttackMs;
	double ReleaseMs;

	//FX
	double EchoDelayS;
	double EchoGainL;
	double EchoGainR;
	double ReverbWet;

	//Mix
	double LeadGain;
	double HarmGain;
	double BassGain;
	double DrumGain;
};

//Experimental "enhanced audio" synth.
//Re-interprets the APU channel state (frequency/volume/duty) with modern
//instrument timbres, mixed on top of (or in place of) the original chip
//output. The APU remains the source of truth: this only *reads* its state.
class EnhancedSynth final : public IAudioProvider
{
private:
	struct Voice
	{
		double Phase = 0;
		double PhaseB = 0;
		double SubPhase = 0;
		double SmoothedVol = 0;
		double Lp = 0;
		double LastFreq = 0;
		double LastVol = 0;
	};

	Emulator* _emu = nullptr;
	NesConsole* _console = nullptr;

	Voice _lead;
	Voice _harmony;
	Voice _bass;
	double _noiseVol = 0;
	uint32_t _noiseRng = 0x1D872B41;

	//Drum tone shaping (one-pole states) + low thump oscillator
	double _drumLpLow = 0;
	double _drumLpHigh = 0;
	double _drumLpTop = 0;
	double _thumpPhase = 0;
	double _thumpGate = 0;
	double _lastNoisePollVol = 0;
	bool _wasActive = false;

	//Lead echo + light feedforward reverb (sized for the 96kHz max rate)
	std::vector<float> _echoBuf = std::vector<float>(32768);
	std::vector<float> _revBufL = std::vector<float>(18432);
	std::vector<float> _revBufR = std::vector<float>(18432);
	uint32_t _echoPos = 0;
	uint32_t _revPos = 0;

	static double PolyBlep(double t, double dt);
	static double BlepSaw(double phase, double inc);
	static void Retrigger(Voice& voice, double freq, double vol);
	static const EnhancedSynthPreset& GetPreset(uint32_t presetId);
	double NextNoise();

public:
	EnhancedSynth(Emulator* emu, NesConsole* console);
	virtual ~EnhancedSynth();

	//Clears delay lines and voice state. Called when the synth is disabled,
	//on console reset and after loading a save state, so no stale audio
	//leaks across those boundaries.
	void Reset();

	void MixAudio(int16_t* out, uint32_t sampleCount, uint32_t sampleRate) override;
};
