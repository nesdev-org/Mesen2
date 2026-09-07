#include "Common.h"
#include "Core/Shared/Emulator.h"
#include "Core/Shared/Interfaces/IAudioDevice.h"
#include "Core/Shared/EmuSettings.h"
#include "Core/Shared/SettingTypes.h"
#include "Utilities/StringUtilities.h"
#include "Utilities/Video/LibrashaderUtilities.h"

extern unique_ptr<Emulator> _emu;

extern "C"
{
	DllExport void __stdcall SetVideoConfig(VideoConfig config)
	{
		_emu->GetSettings()->SetVideoConfig(config);
	}

	DllExport void __stdcall SetShaderConfig(InteropShaderConfig config)
	{
		_emu->GetSettings()->SetShaderConfig(config);
	}

	DllExport void __stdcall SetAudioConfig(AudioConfig config)
	{
		_emu->GetSettings()->SetAudioConfig(config);
	}

	DllExport void __stdcall SetInputConfig(InputConfig config)
	{
		_emu->GetSettings()->SetInputConfig(config);
	}

	DllExport void __stdcall SetEmulationConfig(EmulationConfig config)
	{
		_emu->GetSettings()->SetEmulationConfig(config);
	}

	DllExport void __stdcall SetGameboyConfig(GameboyConfig config)
	{
		_emu->GetSettings()->SetGameboyConfig(config);
	}

	DllExport void __stdcall SetGbaConfig(GbaConfig config)
	{
		_emu->GetSettings()->SetGbaConfig(config);
	}

	DllExport void __stdcall SetPcEngineConfig(PcEngineConfig config)
	{
		_emu->GetSettings()->SetPcEngineConfig(config);
	}

	DllExport void __stdcall SetNesConfig(NesConfig config)
	{
		_emu->GetSettings()->SetNesConfig(config);
	}

	DllExport void __stdcall SetSnesConfig(SnesConfig config)
	{
		_emu->GetSettings()->SetSnesConfig(config);
	}

	DllExport void __stdcall SetSmsConfig(SmsConfig config)
	{
		_emu->GetSettings()->SetSmsConfig(config);
	}

	DllExport void __stdcall SetCvConfig(CvConfig config)
	{
		_emu->GetSettings()->SetCvConfig(config);
	}

	DllExport void __stdcall SetWsConfig(WsConfig config)
	{
		_emu->GetSettings()->SetWsConfig(config);
	}

	DllExport void __stdcall SetGameConfig(GameConfig config)
	{
		_emu->GetSettings()->SetGameConfig(config);
	}

	DllExport void __stdcall SetPreferences(PreferencesConfig config)
	{
		_emu->GetSettings()->SetPreferences(config);
	}

	DllExport void __stdcall SetAudioPlayerConfig(AudioPlayerConfig config)
	{
		_emu->GetSettings()->SetAudioPlayerConfig(config);
	}

	DllExport void __stdcall SetDebugConfig(DebugConfig config)
	{
		_emu->GetSettings()->SetDebugConfig(config);
	}

	DllExport void __stdcall SetShortcutKeys(ShortcutKeyInfo shortcuts[], uint32_t count)
	{
		vector<ShortcutKeyInfo> shortcutList(shortcuts, shortcuts + count);
		_emu->GetSettings()->SetShortcutKeys(shortcutList);
	}

	DllExport NesConfig __stdcall GetNesConfig()
	{
		return _emu->GetSettings()->GetNesConfig();
	}

	DllExport void __stdcall GetAudioDevices(char* outDeviceList, uint32_t maxLength)
	{
		shared_ptr<IAudioDevice> soundManager = _emu->GetSoundManager();
		StringUtilities::CopyToBuffer(soundManager ? soundManager->GetAvailableDevices() : "", outDeviceList, maxLength);
	}

	DllExport void __stdcall SetEmulationFlag(EmulationFlags flag, bool enabled)
	{
		_emu->GetSettings()->SetFlagState(flag, enabled);
	}

	DllExport void __stdcall SetDebuggerFlag(DebuggerFlags flag, bool enabled)
	{
		_emu->GetSettings()->SetDebuggerFlag(flag, enabled);
	}

	DllExport bool __stdcall CheckShaderSupport()
	{
		return LibrashaderUtilities::CheckShaderSupport();
	}

	DllExport uint32_t __stdcall GetShaderParams(const char* shaderFile, ShaderParamDefinition* params)
	{
		if(params) {
			vector<ShaderParamDefinition> paramList = LibrashaderUtilities::GetShaderParams(shaderFile);
			std::copy(paramList.begin(), paramList.end(), params);
			return (uint32_t)paramList.size();
		} else {
			return LibrashaderUtilities::GetShaderParamCount(shaderFile);
		}
	}
}