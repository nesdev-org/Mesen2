#pragma once
#include "pch.h"
#include "DirectSoundManager.h"
#include "WasapiSoundManager.h"

class IAudioDevice;
class Emulator;

class SoundManager
{
public:
	static IAudioDevice* Create(Emulator* emu, HWND hwnd)
	{
		switch(emu->GetSettings()->GetAudioConfig().AudioBackend) {
			default:
			case AudioBackendType::Wasapi: return new WasapiSoundManager(emu);
			case AudioBackendType::DirectSound: return new DirectSoundManager(emu, hwnd);
		}
	}
};