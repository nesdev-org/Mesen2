#pragma once

#include "Common.h"
#include "Core/Shared/Audio/BaseSoundManager.h"
#include <audioclient.h>
#include <wrl/client.h>

class Emulator;

class WasapiSoundManager final : public BaseSoundManager
{
	struct SoundDeviceInfo
	{
		string Description;
		string Id;
	};

private:
	Emulator* _emu;
	bool _needReset = false;

	string _audioDeviceId;
	string _audioDeviceName = "";

	uint32_t _previousLatency = 0;
	bool _playing = false;

	uint32_t _bufferFrameCount = 0;

	Microsoft::WRL::ComPtr<IAudioClient> _audioClient;
	Microsoft::WRL::ComPtr<IAudioRenderClient> _renderClient;

	vector<SoundDeviceInfo> GetAvailableDeviceInfo();
	bool Initialize(uint32_t sampleRate, bool isStereo);
	void LogError(const char* msg, HRESULT hr);

public:
	WasapiSoundManager(Emulator* emu);
	~WasapiSoundManager();

	void Release();
	void ProcessEndOfFrame();
	void PlayBuffer(int16_t* soundBuffer, uint32_t bufferSize, uint32_t sampleRate, bool isStereo);
	void Play();
	void Pause();
	void Stop();

	string GetAvailableDevices();
	void SetAudioDevice(string deviceName);
};
