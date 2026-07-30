#pragma once
#include "pch.h"
#include "Utilities/ISerializable.h"
#include "NES/APU/NesApu.h"
#include "NES/Mappers/FDS/ModChannel.h"
#include "NES/Mappers/FDS/BaseFdsChannel.h"

class NesConsole;
struct MapperStateEntry;

class FdsAudio final : public ISerializable
{
private:
	const uint32_t WaveVolumeTable[4] = { 36, 24, 17, 14 };

	NesConsole* _console = nullptr;
	NesApu* _apu = nullptr;

	//Register values
	uint8_t _waveTable[64] = {};
	bool _waveWriteEnabled = false;

	BaseFdsChannel _volume;
	ModChannel _mod;

	bool _disableEnvelopes = false;
	bool _haltWaveform = false;

	uint8_t _masterVolume = 0;

	//Internal values
	uint16_t _waveOverflowCounter = 0;
	int32_t _wavePitch = 0;
	uint8_t _wavePosition = 0;

	uint8_t _lastOutput = 0;

protected:
	void Serialize(Serializer& s) override;

	void UpdateOutput();

	uint32_t GetWaveAccumulator();

public:
	FdsAudio(NesConsole* console);

	__forceinline void Clock()
	{
		if(!_apu->IsApuEnabled()) {
			return;
		}

		int frequency = _volume.GetFrequency();
		if(!_haltWaveform && !_disableEnvelopes) {
			_volume.TickEnvelope();
			if(_mod.TickEnvelope()) {
				_mod.UpdateOutput(frequency);
			}
		}

		if(_mod.TickModulator()) {
			//Modulator was ticked, update wave pitch
			_mod.UpdateOutput(frequency);
		}

		UpdateOutput();

		if(!_haltWaveform && frequency + _mod.GetOutput() > 0) {
			_waveOverflowCounter += frequency + _mod.GetOutput();
			if(_waveOverflowCounter < frequency + _mod.GetOutput()) {
				_wavePosition = (_wavePosition + 1) & 0x3F;
			}
		}
	}

	uint8_t ReadRegister(uint16_t addr);
	void WriteRegister(uint16_t addr, uint8_t value);

	void GetMapperStateEntries(vector<MapperStateEntry>& entries);
};
