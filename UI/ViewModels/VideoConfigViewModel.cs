using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Mesen.Config;
using Mesen.Interop;
using Mesen.Utilities;
using System;
using System.Linq;

namespace Mesen.ViewModels
{
	public partial class VideoConfigViewModel : DisposableViewModel
	{
		public bool IsWindows { get; }
		public bool IsWindows10 { get; }
		public bool IsMacOs { get; }
		public bool ShowShaderConfig { get; }

		public IRelayCommand PresetCompositeCommand { get; }
		public IRelayCommand PresetSVideoCommand { get; }
		public IRelayCommand PresetRgbCommand { get; }
		public IRelayCommand PresetMonochromeCommand { get; }
		public IRelayCommand ResetPictureSettingsCommand { get; }

		[ObservableProperty] public partial VideoConfig Config { get; set; }
		[ObservableProperty] public partial VideoConfig OriginalConfig { get; set; }
		public UInt32[] AvailableRefreshRates { get; } = new UInt32[] { 50, 60, 75, 100, 120, 144, 200, 240, 360 };

		public VideoConfigViewModel()
		{
			Config = ConfigManager.Config.Video;
			OriginalConfig = Config.Clone();

			PresetCompositeCommand = new RelayCommand(() => SetNtscPreset(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, false));
			PresetSVideoCommand = new RelayCommand(() => SetNtscPreset(0, 0, 0, 0, 20, 0, 20, -100, -100, 0, 15, false));
			PresetRgbCommand = new RelayCommand(() => SetNtscPreset(0, 0, 0, 0, 20, 0, 70, -100, -100, -100, 15, false));
			PresetMonochromeCommand = new RelayCommand(() => SetNtscPreset(0, -100, 0, 0, 20, 0, 70, -20, -20, -10, 15, false));
			ResetPictureSettingsCommand = new RelayCommand(() => ResetPictureSettings());

			AddDisposable(Config.ObserveProp(nameof(VideoConfig.UseSoftwareRenderer), () => {
				if(Config.UseSoftwareRenderer) {
					//Not supported
					Config.UseExclusiveFullscreen = false;
					Config.VerticalSync = false;
				}
			}));

			//Exclusive fullscreen is only supported on Windows currently
			IsWindows = OperatingSystem.IsWindows();
			IsWindows10 = OperatingSystem.IsWindowsVersionAtLeast(10);

			//MacOS only supports the software renderer
			IsMacOs = OperatingSystem.IsMacOS();

			ShowShaderConfig = ConfigApi.CheckShaderSupport();

			if(Design.IsDesignMode) {
				return;
			}

			AddDisposable(ReactiveHelper.RegisterRecursiveObserver(Config, (s, e) => { Config.ApplyConfig(); }));
		}

		private void SetNtscPreset(int hue, int saturation, int contrast, int brightness, int sharpness, int gamma, int resolution, int artifacts, int fringing, int bleed, int scanlines, bool mergeFields)
		{
			Config.VideoFilter = VideoFilterType.NtscBlargg;
			Config.Hue = hue;
			Config.Saturation = saturation;
			Config.Contrast = contrast;
			Config.Brightness = brightness;
			Config.NtscSharpness = sharpness;
			Config.NtscGamma = gamma;
			Config.NtscResolution = resolution;
			Config.NtscArtifacts = artifacts;
			Config.NtscFringing = fringing;
			Config.NtscBleed = bleed;
			Config.NtscMergeFields = mergeFields;

			Config.ScanlineIntensity = scanlines;
		}

		private void ResetPictureSettings()
		{
			SetNtscPreset(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false);
			Config.NtscScale = NtscBisqwitFilterScale._2x;
			Config.NtscYFilterLength = 0;
			Config.NtscIFilterLength = 50;
			Config.NtscQFilterLength = 50;
			Config.VideoFilter = VideoFilterType.None;

			Config.LcdGridTopLeftBrightness = 100;
			Config.LcdGridTopRightBrightness = 85;
			Config.LcdGridBottomLeftBrightness = 85;
			Config.LcdGridBottomRightBrightness = 85;
		}
	}
}
