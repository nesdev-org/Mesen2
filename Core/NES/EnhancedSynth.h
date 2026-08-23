#pragma once
#include "pch.h"
#include <fstream>
#include "Shared/Interfaces/IAudioProvider.h"

class Emulator;
class NesConsole;

//Instrument definition for the enhanced audio synth. Built-in defaults are
//defined in EnhancedSynth.cpp; any field can be overridden per-preset, with
//no rebuild, via a "EnhancedAudioPresets.cfg" text file in the Mesen home
//folder (see EnhancedSynth::LoadUserPresets for the format).
struct EnhancedSynthPreset
{
	//Pulse voices
	double LeadDetune;
	double HarmDetune;
	bool FollowDuty;        //pulse width follows the game's duty setting
	double FixedWidth;      //used when FollowDuty is false
	bool LeadAlwaysSaw;     //true: lead is a fixed detuned-saw stack, duty ignored entirely
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

	//Master bus soft-compressor (approximates the offline normalize+tanh
	//master); CompThreshold == 0 disables it entirely (zero extra cost).
	double CompThreshold;
	double CompRatio;
	double CompAttackMs;
	double CompReleaseMs;
	double CompMakeup;
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

	//Master bus compressor envelope (linked stereo: one detector for L+R)
	double _compEnv = 0;

	//Built-in presets (EnhancedSynth.cpp's _presets) with any overrides from
	//EnhancedAudioPresets.cfg applied on top; reloaded in the constructor and
	//in Reset(), so toggling "Enable enhanced audio" off/on re-reads the file.
	EnhancedSynthPreset _userPresets[5];

	//Debug tap (temporary, diagnostic only): enabled by the presence of a
	//"synthdebug" marker file in the Mesen home folder. Dumps the incoming
	//buffer, the outgoing buffer and a per-flush state CSV for offline
	//analysis of stutter/click reports.
	int _dbgState = -1; //-1 = not checked yet, 0 = off, 1 = on
	std::ofstream _dbgCsv;
	std::ofstream _dbgIn;
	std::ofstream _dbgOut;
	uint64_t _dbgSampleOffset = 0;
	uint32_t _dbgFlush = 0;

	static double PolyBlep(double t, double dt);
	static double BlepSaw(double phase, double inc);
	static void Retrigger(Voice& voice, double freq, double vol);
	const EnhancedSynthPreset& GetPreset(uint32_t presetId) const;
	double NextNoise();
	void InitDebugTap();
	void LoadUserPresets();

public:
	EnhancedSynth(Emulator* emu, NesConsole* console);
	virtual ~EnhancedSynth();

	//Clears delay lines and voice state. Called when the synth is disabled
	//and on console reset, so no stale audio leaks across those boundaries.
	//Deliberately NOT called on state load (run-ahead deserializes per frame).
	void Reset();

	void MixAudio(int16_t* out, uint32_t sampleCount, uint32_t sampleRate) override;
};
