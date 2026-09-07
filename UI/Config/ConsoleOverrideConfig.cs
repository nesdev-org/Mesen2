using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Interop;
using Mesen.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Mesen.Config;

public partial class ConsoleOverrideConfig : BaseConfig<GameConfig>
{
	[ObservableProperty] public partial bool OverrideVideoFilter { get; set; } = false;
	[ObservableProperty] public partial VideoFilterType VideoFilter { get; set; } = VideoFilterType.None;

	[ObservableProperty] public partial bool OverrideShader { get; set; } = false;
	[ObservableProperty] public partial string ShaderFile { get; set; } = "";

	[ObservableProperty] public partial bool OverrideAspectRatio { get; set; } = false;
	[ObservableProperty] public partial VideoAspectRatio AspectRatio { get; set; } = VideoAspectRatio.NoStretching;
	[ObservableProperty][MinMax(0.1, 5.0)] public partial double CustomAspectRatio { get; set; } = 1.0;

	public static ConsoleOverrideConfig? GetActiveOverride()
	{
		if(MainWindowViewModel.Instance == null) {
			return null;
		}

		RomInfo romInfo = MainWindowViewModel.Instance.RomInfo;
		if(romInfo.Format == RomFormat.Unknown) {
			return null;
		}

		switch(romInfo.ConsoleType) {
			case ConsoleType.Snes: return ConfigManager.Config.Snes.ConfigOverrides;
			case ConsoleType.Gameboy: return ConfigManager.Config.Gameboy.ConfigOverrides;
			case ConsoleType.Nes: return ConfigManager.Config.Nes.ConfigOverrides;
			case ConsoleType.PcEngine: return ConfigManager.Config.PcEngine.ConfigOverrides;
			case ConsoleType.Sms:
				if(romInfo.Format == RomFormat.ColecoVision) {
					return ConfigManager.Config.Cv.ConfigOverrides;
				} else if(romInfo.Format == RomFormat.GameGear) {
					return ConfigManager.Config.Sms.GgConfigOverrides;
				} else {
					return ConfigManager.Config.Sms.ConfigOverrides;
				}
			case ConsoleType.Gba: return ConfigManager.Config.Gba.ConfigOverrides;
			case ConsoleType.Ws: return ConfigManager.Config.Ws.ConfigOverrides;
		}

		return null;
	}
}
