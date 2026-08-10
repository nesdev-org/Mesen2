using Avalonia;
using Avalonia.Markup.Xaml.Styling;
using Avalonia.Media;
using Avalonia.Styling;
using Avalonia.Threading;
using Mesen.Config.Shortcuts;
using Mesen.Interop;
using Mesen.Localization;
using Mesen.Utilities;
using Mesen.ViewModels;
using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Mesen.Config
{
	public partial class PreferencesConfig : BaseConfig<PreferencesConfig>
	{
		[ObservableProperty] public partial MesenTheme Theme { get; set; } = MesenTheme.Light;
		[ObservableProperty] public partial MesenLanguage Language { get; set; } = MesenLanguage.English;
		[ObservableProperty] public partial bool AutomaticallyCheckForUpdates { get; set; } = true;
		[ObservableProperty] public partial bool SingleInstance { get; set; } = true;
		[ObservableProperty] public partial bool AutoLoadPatches { get; set; } = true;

		[ObservableProperty] public partial bool PauseWhenInBackground { get; set; } = false;
		[ObservableProperty] public partial bool PauseWhenInMenusAndConfig { get; set; } = false;
		[ObservableProperty] public partial bool AllowBackgroundInput { get; set; } = false;
		[ObservableProperty] public partial bool PauseOnMovieEnd { get; set; } = true;
		[ObservableProperty] public partial bool ShowMovieIcons { get; set; } = true;
		[ObservableProperty] public partial bool ShowTurboRewindIcons { get; set; } = true;
		[ObservableProperty] public partial bool ConfirmExitResetPower { get; set; } = false;

		[ObservableProperty] public partial bool AssociateSnesRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateSnesMusicFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateNesRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateNesMusicFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateGbRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateGbMusicFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateGbaRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociatePceRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociatePceMusicFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateSmsRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateGameGearRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateSgRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateCvRomFiles { get; set; } = false;
		[ObservableProperty] public partial bool AssociateWsRomFiles { get; set; } = false;

		[ObservableProperty] public partial bool EnableAutoSaveState { get; set; } = true;
		[ObservableProperty] public partial UInt32 AutoSaveStateDelay { get; set; } = 5;

		[ObservableProperty] public partial bool EnableRewind { get; set; } = true;
		[ObservableProperty] public partial UInt32 RewindBufferSize { get; set; } = 300;

		[ObservableProperty] public partial bool AlwaysOnTop { get; set; } = false;

		[ObservableProperty] public partial bool AutoHideMenu { get; set; } = false;

		[ObservableProperty] public partial bool ShowFps { get; set; } = false;
		[ObservableProperty] public partial bool ShowFrameCounter { get; set; } = false;
		[ObservableProperty] public partial bool ShowGameTimer { get; set; } = false;
		[ObservableProperty] public partial bool ShowLagCounter { get; set; } = false;
		[ObservableProperty] public partial bool ShowTitleBarInfo { get; set; } = false;
		[ObservableProperty] public partial bool ShowDebugInfo { get; set; } = false;
		[ObservableProperty] public partial bool DisableOsd { get; set; } = false;
		[ObservableProperty] public partial HudDisplaySize HudSize { get; set; } = HudDisplaySize.Fixed;
		[ObservableProperty] public partial GameSelectionMode GameSelectionScreenMode { get; set; } = GameSelectionMode.ResumeState;

		[ObservableProperty] public partial FontAntialiasing FontAntialiasing { get; set; } = FontAntialiasing.SubPixelAntialias;
		[ObservableProperty] public partial FontConfig MesenFont { get; set; } = new FontConfig() { FontFamily = "Microsoft Sans Serif", FontSize = 11 };
		[ObservableProperty] public partial FontConfig MesenMenuFont { get; set; } = new FontConfig() { FontFamily = "Segoe UI", FontSize = 12 };

		[ObservableProperty] public partial List<ShortcutKeyInfo> ShortcutKeys { get; set; } = new List<ShortcutKeyInfo>();

		[ObservableProperty] public partial bool OverrideGameFolder { get; set; } = false;
		[ObservableProperty] public partial bool OverrideAviFolder { get; set; } = false;
		[ObservableProperty] public partial bool OverrideMovieFolder { get; set; } = false;
		[ObservableProperty] public partial bool OverrideSaveDataFolder { get; set; } = false;
		[ObservableProperty] public partial bool OverrideSaveStateFolder { get; set; } = false;
		[ObservableProperty] public partial bool OverrideScreenshotFolder { get; set; } = false;
		[ObservableProperty] public partial bool OverrideWaveFolder { get; set; } = false;

		[ObservableProperty] public partial string GameFolder { get; set; } = "";
		[ObservableProperty] public partial string AviFolder { get; set; } = "";
		[ObservableProperty] public partial string MovieFolder { get; set; } = "";
		[ObservableProperty] public partial string SaveDataFolder { get; set; } = "";
		[ObservableProperty] public partial string SaveStateFolder { get; set; } = "";
		[ObservableProperty] public partial string ScreenshotFolder { get; set; } = "";
		[ObservableProperty] public partial string WaveFolder { get; set; } = "";

		public PreferencesConfig()
		{
		}

		private void AddShortcut(ShortcutKeyInfo shortcut)
		{
			if(!ShortcutKeys.Exists(a => a.Shortcut == shortcut.Shortcut)) {
				ShortcutKeys.Add(shortcut);
			}
		}

		public void InitializeDefaultShortcuts()
		{
			UInt16 ctrl = InputApi.GetKeyCode("Left Ctrl");
			UInt16 alt = InputApi.GetKeyCode("Left Alt");
			UInt16 shift = InputApi.GetKeyCode("Left Shift");

			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.FastForward, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("Tab") }, KeyCombination2 = new KeyCombination() { Key1 = InputApi.GetKeyCode("Pad1 R2") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.Rewind, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("Backspace") }, KeyCombination2 = new KeyCombination() { Key1 = InputApi.GetKeyCode("Pad1 L2") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.IncreaseSpeed, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("=") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.DecreaseSpeed, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("-") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.MaxSpeed, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F9") } });

			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.IncreaseVolume, KeyCombination = new KeyCombination() { Key1 = ctrl, Key2 = InputApi.GetKeyCode("=") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.DecreaseVolume, KeyCombination = new KeyCombination() { Key1 = ctrl, Key2 = InputApi.GetKeyCode("-") } });

			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.ToggleFps, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F10") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.ToggleFullscreen, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F11") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.TakeScreenshot, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F12") } });

			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.Reset, KeyCombination = new KeyCombination() { Key1 = ctrl, Key2 = InputApi.GetKeyCode("R") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.PowerCycle, KeyCombination = new KeyCombination() { Key1 = ctrl, Key2 = InputApi.GetKeyCode("T") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.ReloadRom, KeyCombination = new KeyCombination() { Key1 = ctrl, Key2 = shift, Key3 = InputApi.GetKeyCode("R") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.Pause, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("Esc") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.RunSingleFrame, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("`") } });

			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale1x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("1") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale2x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("2") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale3x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("3") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale4x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("4") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale5x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("5") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale6x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("6") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale7x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("7") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale8x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("8") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale9x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("9") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SetScale10x, KeyCombination = new KeyCombination() { Key1 = alt, Key2 = InputApi.GetKeyCode("0") } });

			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.OpenFile, KeyCombination = new KeyCombination() { Key1 = ctrl, Key2 = InputApi.GetKeyCode("O") } });

			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SaveStateSlot1, KeyCombination = new KeyCombination() { Key1 = shift, Key2 = InputApi.GetKeyCode("F1") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SaveStateSlot2, KeyCombination = new KeyCombination() { Key1 = shift, Key2 = InputApi.GetKeyCode("F2") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SaveStateSlot3, KeyCombination = new KeyCombination() { Key1 = shift, Key2 = InputApi.GetKeyCode("F3") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SaveStateSlot4, KeyCombination = new KeyCombination() { Key1 = shift, Key2 = InputApi.GetKeyCode("F4") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SaveStateSlot5, KeyCombination = new KeyCombination() { Key1 = shift, Key2 = InputApi.GetKeyCode("F5") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SaveStateSlot6, KeyCombination = new KeyCombination() { Key1 = shift, Key2 = InputApi.GetKeyCode("F6") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SaveStateSlot7, KeyCombination = new KeyCombination() { Key1 = shift, Key2 = InputApi.GetKeyCode("F7") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.SaveStateToFile, KeyCombination = new KeyCombination() { Key1 = ctrl, Key2 = InputApi.GetKeyCode("S") } });

			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateSlot1, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F1") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateSlot2, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F2") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateSlot3, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F3") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateSlot4, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F4") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateSlot5, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F5") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateSlot6, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F6") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateSlot7, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F7") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateSlotAuto, KeyCombination = new KeyCombination() { Key1 = InputApi.GetKeyCode("F8") } });
			AddShortcut(new ShortcutKeyInfo { Shortcut = EmulatorShortcut.LoadStateFromFile, KeyCombination = new KeyCombination() { Key1 = ctrl, Key2 = InputApi.GetKeyCode("L") } });

			foreach(EmulatorShortcut value in Enum.GetValues<EmulatorShortcut>()) {
				if(value < EmulatorShortcut.LastValidValue) {
					AddShortcut(new ShortcutKeyInfo { Shortcut = value });
				}
			}
		}

		public void UpdateFileAssociations()
		{
			FileAssociationHelper.UpdateFileAssociations();
		}

		public void ApplyFontOptions()
		{
			UpdateFonts();
		}

		private void UpdateFonts()
		{
			if(Application.Current != null) {
				string mesenFont = Configuration.GetValidFontFamily(MesenFont.FontFamily, false);
				string menuFont = Configuration.GetValidFontFamily(MesenMenuFont.FontFamily, false);

				if(Application.Current.Resources["MesenFont"] is FontFamily curMesenFont && curMesenFont.Name != mesenFont) {
					Application.Current.Resources["MesenFont"] = new FontFamily(mesenFont);
				}
				if(Application.Current.Resources["MesenMenuFont"] is FontFamily curMesenMenuFont && curMesenMenuFont.Name != menuFont) {
					Application.Current.Resources["MesenMenuFont"] = new FontFamily(menuFont);
				}

				if(Application.Current.Resources["MesenFontSize"] is double curMesenFontSize && curMesenFontSize != MesenFont.FontSize) {
					Application.Current.Resources["MesenFontSize"] = (double)MesenFont.FontSize;
				}
				if(Application.Current.Resources["MesenMenuFontSize"] is double curMesenMenuFontSize && curMesenMenuFontSize != MesenMenuFont.FontSize) {
					Application.Current.Resources["MesenMenuFontSize"] = (double)MesenMenuFont.FontSize;
				}
			}
		}

		public void InitializeFontDefaults()
		{
			MesenFont = Configuration.GetDefaultFont();
			MesenMenuFont = Configuration.GetDefaultMenuFont();
			ApplyFontOptions();
		}

		public static void UpdateTheme()
		{
			if(Application.Current != null) {
				ThemeVariant newTheme = ConfigManager.Config.Preferences.Theme == MesenTheme.Dark ? ThemeVariant.Dark : ThemeVariant.Light;
				if(Application.Current.RequestedThemeVariant != newTheme) {
					ConfigManager.ActiveTheme = ConfigManager.Config.Preferences.Theme;
					Application.Current.RequestedThemeVariant = newTheme;
				}
			}
		}

		public void ApplyConfig()
		{
			UpdateFonts();

			List<InteropShortcutKeyInfo> shortcutKeys = new List<InteropShortcutKeyInfo>();
			foreach(ShortcutKeyInfo shortcutInfo in ShortcutKeys) {
				if(!shortcutInfo.KeyCombination.IsEmpty) {
					shortcutKeys.Add(new InteropShortcutKeyInfo(shortcutInfo.Shortcut, shortcutInfo.KeyCombination.ToInterop()));
				}
				if(!shortcutInfo.KeyCombination2.IsEmpty) {
					shortcutKeys.Add(new InteropShortcutKeyInfo(shortcutInfo.Shortcut, shortcutInfo.KeyCombination2.ToInterop()));
				}
			}
			ConfigApi.SetShortcutKeys(shortcutKeys.ToArray(), (UInt32)shortcutKeys.Count);

			ConfigApi.SetPreferences(new InteropPreferencesConfig() {
				ShowFps = ShowFps,
				ShowFrameCounter = ShowFrameCounter,
				ShowGameTimer = ShowGameTimer,
				ShowDebugInfo = ShowDebugInfo,
				ShowLagCounter = ShowLagCounter,
				DisableOsd = DisableOsd,
				AllowBackgroundInput = AllowBackgroundInput,
				PauseOnMovieEnd = PauseOnMovieEnd,
				ShowMovieIcons = ShowMovieIcons,
				ShowTurboRewindIcons = ShowTurboRewindIcons,
				DisableGameSelectionScreen = GameSelectionScreenMode == GameSelectionMode.Disabled,
				HudSize = HudSize,
				SaveFolderOverride = OverrideSaveDataFolder ? SaveDataFolder : "",
				SaveStateFolderOverride = OverrideSaveStateFolder ? SaveStateFolder : "",
				ScreenshotFolderOverride = OverrideScreenshotFolder ? ScreenshotFolder : "",
				RewindBufferSize = EnableRewind ? RewindBufferSize : 0,
				AutoSaveStateDelay = EnableAutoSaveState ? AutoSaveStateDelay : 0
			});
		}
	}

	public enum MesenTheme
	{
		Light = 0,
		Dark = 1
	}

	public enum MesenLanguage
	{
		English = 0,
		Chinese = 1
	}

	public enum FontAntialiasing
	{
		Disabled,
		Antialias,
		SubPixelAntialias
	}

	public enum GameSelectionMode
	{
		Disabled,
		ResumeState,
		PowerOn
	}

	public enum HudDisplaySize
	{
		Fixed,
		Scaled,
	}

	public struct InteropPreferencesConfig
	{
		[MarshalAs(UnmanagedType.I1)] public bool ShowFps;
		[MarshalAs(UnmanagedType.I1)] public bool ShowFrameCounter;
		[MarshalAs(UnmanagedType.I1)] public bool ShowGameTimer;
		[MarshalAs(UnmanagedType.I1)] public bool ShowLagCounter;
		[MarshalAs(UnmanagedType.I1)] public bool ShowDebugInfo;
		[MarshalAs(UnmanagedType.I1)] public bool DisableOsd;
		[MarshalAs(UnmanagedType.I1)] public bool AllowBackgroundInput;
		[MarshalAs(UnmanagedType.I1)] public bool PauseOnMovieEnd;
		[MarshalAs(UnmanagedType.I1)] public bool ShowMovieIcons;
		[MarshalAs(UnmanagedType.I1)] public bool ShowTurboRewindIcons;
		[MarshalAs(UnmanagedType.I1)] public bool DisableGameSelectionScreen;

		public HudDisplaySize HudSize;

		public UInt32 AutoSaveStateDelay;
		public UInt32 RewindBufferSize;

		public string SaveFolderOverride;
		public string SaveStateFolderOverride;
		public string ScreenshotFolderOverride;
	}
}
