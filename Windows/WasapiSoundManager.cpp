#include "Common.h"
#include "WasapiSoundManager.h"
#include "Core/Shared/Audio/SoundMixer.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/MessageManager.h"
#include "Utilities/UTF8Util.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

WasapiSoundManager::WasapiSoundManager(Emulator* emu)
{
	_emu = emu;

	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if(Initialize(48000, true)) {
		_emu->GetSoundMixer()->RegisterAudioDevice(this);
	} else {
		Release();
		MessageManager::DisplayMessage("Error", "CouldNotInitializeAudioSystem");
	}
}

WasapiSoundManager::~WasapiSoundManager()
{
	if(_emu && _emu->GetSoundMixer()) {
		_emu->GetSoundMixer()->RegisterAudioDevice(nullptr);
	}
	Release();
	CoUninitialize();
}

vector<WasapiSoundManager::SoundDeviceInfo> WasapiSoundManager::GetAvailableDeviceInfo()
{
	ComPtr<IMMDeviceEnumerator> enumerator;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
	if(FAILED(hr)) {
		LogError("IMMDeviceEnumerator creation failed.", hr);
		return {};
	}

	ComPtr<IMMDeviceCollection> collection;
	hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
	if(FAILED(hr)) {
		LogError("EnumAudioEndpoints failed.", hr);
		return {};
	}

	UINT count = 0;
	collection->GetCount(&count);

	vector<SoundDeviceInfo> devices;

	for(UINT i = 0; i < count; i++) {
		ComPtr<IMMDevice> device;
		if(SUCCEEDED(collection->Item(i, &device))) {
			LPWSTR deviceId = nullptr;
			if(SUCCEEDED(device->GetId(&deviceId))) {
				ComPtr<IPropertyStore> props;
				if(SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
					PROPVARIANT varName;
					PropVariantInit(&varName);
					if(SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
						SoundDeviceInfo info;
						info.Description = utf8::utf8::encode(varName.pwszVal);
						info.Id = utf8::utf8::encode(deviceId);
						devices.push_back(info);
						PropVariantClear(&varName);
					}
				}
				CoTaskMemFree(deviceId);
			}
		}
	}

	return devices;
}

string WasapiSoundManager::GetAvailableDevices()
{
	string deviceString = "Default||";
	for(SoundDeviceInfo device : GetAvailableDeviceInfo()) {
		deviceString += device.Description + "||"s;
	}
	return deviceString;
}

void WasapiSoundManager::SetAudioDevice(string deviceName)
{
	if(_audioDeviceName != deviceName) {
		if(deviceName == "Default") {
			_audioDeviceName = deviceName;
			_audioDeviceId = "";
			_needReset = true;
		} else {
			for(SoundDeviceInfo& device : GetAvailableDeviceInfo()) {
				if(device.Description == deviceName) {
					_audioDeviceName = deviceName;
					if(_audioDeviceId != device.Id) {
						_audioDeviceId = device.Id;
						_needReset = true;
					}
					break;
				}
			}
		}
	}
}

void WasapiSoundManager::LogError(const char* msg, HRESULT hr)
{
	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		hr,
		LANG_USER_DEFAULT,
		(LPSTR)&messageBuffer,
		0,
		NULL);

	string errorMsg(messageBuffer, size);
	LocalFree(messageBuffer);
	MessageManager::Log("[WASAPI] " + string(msg) + " Error: " + errorMsg + " (" + std::to_string(hr) + ")");
}

bool WasapiSoundManager::Initialize(uint32_t sampleRate, bool isStereo)
{
	HRESULT hr;
	_sampleRate = sampleRate;
	_isStereo = isStereo;

	ComPtr<IMMDeviceEnumerator> enumerator;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
	if(FAILED(hr)) {
		LogError("IMMDeviceEnumerator creation failed.", hr);
		return false;
	}

	ComPtr<IMMDevice> device;
	if(!_audioDeviceId.empty()) {
		std::wstring deviceId = utf8::utf8::decode(_audioDeviceId);
		hr = enumerator->GetDevice(deviceId.c_str(), &device);
		if(FAILED(hr)) {
			LogError("IMMDeviceEnumerator::GetDevice failed.", hr);
		}
	}

	if(!device) {
		hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
		if(FAILED(hr)) {
			LogError("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed.", hr);
		}
	}

	hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&_audioClient);
	if(FAILED(hr)) {
		LogError("IAudioClient::Activate failed.", hr);
		return false;
	}

	WAVEFORMATEX waveFormat = {};
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = isStereo ? 2 : 1;
	waveFormat.nSamplesPerSec = sampleRate;
	waveFormat.wBitsPerSample = 16;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
	waveFormat.cbSize = 0;

	//Make the buffer at least 200ms long, or 2x the size of the requested latency (whichever is bigger)
	int32_t latency = _emu->GetSettings()->GetAudioConfig().AudioLatency;
	REFERENCE_TIME bufferDuration = (REFERENCE_TIME)std::max(100, latency) * 10000 * 2;

	//Init in shared mode
	hr = _audioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
		bufferDuration,
		0,
		&waveFormat,
		NULL);

	if(FAILED(hr)) {
		LogError("IAudioClient::Initialize failed.", hr);
		return false;
	}

	hr = _audioClient->GetBufferSize(&_bufferFrameCount);
	if(FAILED(hr)) {
		LogError("IAudioClient::GetBufferSize failed.", hr);
		return false;
	}
	//Used to display buffer size in debug overlay
	_bufferSize = _bufferFrameCount * waveFormat.nBlockAlign;

	hr = _audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&_renderClient);
	if(FAILED(hr)) {
		LogError("IAudioClient::GetService failed.", hr);
		return false;
	}

	return true;
}

void WasapiSoundManager::Release()
{
	Stop();
	_renderClient.Reset();
	_audioClient.Reset();
	_needReset = false;
}

void WasapiSoundManager::Pause()
{
	if(_playing && _audioClient) {
		_audioClient->Stop();
		_playing = false;
	}
}

void WasapiSoundManager::Stop()
{
	if(_audioClient) {
		_audioClient->Stop();
		_audioClient->Reset();
		_playing = false;
	}
	ResetStats();
}

void WasapiSoundManager::Play()
{
	if(!_playing && _audioClient && SUCCEEDED(_audioClient->Start())) {
		_playing = true;
	}
}

void WasapiSoundManager::ProcessEndOfFrame()
{
	AudioConfig cfg = _emu->GetSettings()->GetAudioConfig();
	SetAudioDevice(cfg.AudioDevice);

	if(_audioClient) {
		UINT32 paddingFrames = 0;
		HRESULT hr = _audioClient->GetCurrentPadding(&paddingFrames);

		if(SUCCEEDED(hr)) {
			uint32_t bytesPerFrame = (_isStereo ? 2 : 1) * sizeof(int16_t);
			uint32_t paddingBytes = paddingFrames * bytesPerFrame;
			ProcessLatency(0, paddingBytes);
		} else {
			LogError("IAudioClient::GetCurrentPadding failed.", hr);
		}

		uint32_t emulationSpeed = _emu->GetSettings()->GetEmulationSpeed();
		if(_averageLatency > 0 && emulationSpeed <= 100 && emulationSpeed > 0 && std::abs(_averageLatency - cfg.AudioLatency) > 50) {
			//Latency is way off (over 50ms gap), stop audio & start again
			Stop();
		}
	}

	if(_needReset) {
		Release();
		if(!Initialize(_sampleRate, _isStereo)) {
			Release();
		}
	}
}

void WasapiSoundManager::PlayBuffer(int16_t* soundBuffer, uint32_t sampleCount, uint32_t sampleRate, bool isStereo)
{
	if(!_audioClient) {
		return;
	}

	uint32_t latency = _emu->GetSettings()->GetAudioConfig().AudioLatency;
	if(_sampleRate != sampleRate || _isStereo != isStereo || _needReset || latency != _previousLatency) {
		_previousLatency = latency;
		Release();
		if(!Initialize(sampleRate, isStereo)) {
			Release();
		}
	}

	UINT32 paddingFrames = 0;
	HRESULT hr = _audioClient->GetCurrentPadding(&paddingFrames);
	if(FAILED(hr)) {
		LogError("IAudioClient::GetCurrentPadding failed.", hr);
		return;
	}

	if(paddingFrames == 0) {
		//Out of frames in the buffer, assume this caused a buffer underrun / audio glitch
		_bufferUnderrunEventCount++;
	}

	UINT32 availableFrames = _bufferFrameCount - paddingFrames;
	UINT32 framesToWrite = std::min(sampleCount, availableFrames);

	if(framesToWrite > 0) {
		uint8_t* data = nullptr;
		hr = _renderClient->GetBuffer(framesToWrite, &data);
		if(SUCCEEDED(hr)) {
			uint32_t bytesPerFrame = (isStereo ? 2 : 1) * sizeof(int16_t);
			memcpy(data, soundBuffer, framesToWrite * bytesPerFrame);
			_renderClient->ReleaseBuffer(framesToWrite, 0);
		} else {
			LogError("IAudioRenderClient::GetBuffer failed.", hr);
		}
	}

	if(!_playing) {
		uint32_t sampleLatency = (uint32_t)((float)(sampleRate * latency) / 1000.0f);
		_audioClient->GetCurrentPadding(&paddingFrames);
		if(paddingFrames > sampleLatency) {
			Play();
		}
	}
}
