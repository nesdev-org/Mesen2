using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Interop;
using Mesen.Utilities;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Mesen.Config
{
	public partial class VideoConfig : BaseConfig<VideoConfig>
	{
		[ObservableProperty] public partial string ShaderFile { get; set; } = "";

		[ObservableProperty][MinMax(0.1, 5.0)] public partial double CustomAspectRatio { get; set; } = 1.0;
		[ObservableProperty] public partial VideoFilterType VideoFilter { get; set; } = VideoFilterType.None;
		[ObservableProperty] public partial VideoAspectRatio AspectRatio { get; set; } = VideoAspectRatio.NoStretching;

		[ObservableProperty] public partial bool UseBilinearInterpolation { get; set; } = false;
		[ObservableProperty] public partial bool UseSoftwareRenderer { get; set; } = false;
		[ObservableProperty] public partial bool UseSrgbTextureFormat { get; set; } = false;
		[ObservableProperty] public partial bool VerticalSync { get; set; } = false;
		[ObservableProperty] public partial bool IntegerFpsMode { get; set; } = false;

		[ObservableProperty][MinMax(-100, 100)] public partial int Brightness { get; set; } = 0;
		[ObservableProperty][MinMax(-100, 100)] public partial int Contrast { get; set; } = 0;
		[ObservableProperty][MinMax(-100, 100)] public partial int Hue { get; set; } = 0;
		[ObservableProperty][MinMax(-100, 100)] public partial int Saturation { get; set; } = 0;
		[ObservableProperty][MinMax(0, 100)] public partial int ScanlineIntensity { get; set; } = 0;

		[ObservableProperty][MinMax(0, 100)] public partial int LcdGridTopLeftBrightness { get; set; } = 100;
		[ObservableProperty][MinMax(0, 100)] public partial int LcdGridTopRightBrightness { get; set; } = 85;
		[ObservableProperty][MinMax(0, 100)] public partial int LcdGridBottomLeftBrightness { get; set; } = 85;
		[ObservableProperty][MinMax(0, 100)] public partial int LcdGridBottomRightBrightness { get; set; } = 85;

		[ObservableProperty][MinMax(-100, 100)] public partial int NtscArtifacts { get; set; } = 0;
		[ObservableProperty][MinMax(-100, 100)] public partial int NtscBleed { get; set; } = 0;
		[ObservableProperty][MinMax(-100, 100)] public partial int NtscFringing { get; set; } = 0;
		[ObservableProperty][MinMax(-100, 100)] public partial int NtscGamma { get; set; } = 0;
		[ObservableProperty][MinMax(-100, 100)] public partial int NtscResolution { get; set; } = 0;
		[ObservableProperty][MinMax(-100, 100)] public partial int NtscSharpness { get; set; } = 0;
		[ObservableProperty] public partial bool NtscMergeFields { get; set; } = false;

		[ObservableProperty] public partial NtscBisqwitFilterScale NtscScale { get; set; } = NtscBisqwitFilterScale._2x;
		[ObservableProperty][MinMax(-50, 400)] public partial Int32 NtscYFilterLength { get; set; } = 0;
		[ObservableProperty][MinMax(0, 400)] public partial Int32 NtscIFilterLength { get; set; } = 50;
		[ObservableProperty][MinMax(0, 400)] public partial Int32 NtscQFilterLength { get; set; } = 50;

		[ObservableProperty] public partial bool EnableVariableRefreshRate { get; set; } = false;
		[ObservableProperty] public partial bool FullscreenForceIntegerScale { get; set; } = false;
		[ObservableProperty] public partial bool UseExclusiveFullscreen { get; set; } = false;
		[ObservableProperty] public partial UInt32 ExclusiveFullscreenRefreshRateNtsc { get; set; } = 60;
		[ObservableProperty] public partial UInt32 ExclusiveFullscreenRefreshRatePal { get; set; } = 50;
		[ObservableProperty] public partial FullscreenResolution ExclusiveFullscreenResolution { get; set; } = 0;

		[ObservableProperty] public partial ScreenRotation ScreenRotation { get; set; } = ScreenRotation.None;
		[ObservableProperty] public partial bool DisableHighPrecisionFramePacing { get; set; } = false;

		public VideoConfig()
		{
		}

		public void ApplyConfig()
		{
			double customAspectRatio = CustomAspectRatio;
			VideoAspectRatio aspectRatio = AspectRatio;
			VideoFilterType videoFilter = VideoFilter;
			string shaderFile = ShaderFile;

			ConsoleOverrideConfig? overrides = ConsoleOverrideConfig.GetActiveOverride();
			if(overrides?.OverrideVideoFilter == true) {
				videoFilter = overrides.VideoFilter;
			}

			if(overrides?.OverrideShader == true) {
				shaderFile = overrides.ShaderFile;
			}

			if(overrides?.OverrideAspectRatio == true) {
				aspectRatio = overrides.AspectRatio;
				customAspectRatio = overrides.CustomAspectRatio;
			}

			ShaderConfig.LoadConfig(shaderFile).ApplyConfig();

			ConfigApi.SetVideoConfig(new InteropVideoConfig() {
				CustomAspectRatio = customAspectRatio,
				VideoFilter = videoFilter,
				AspectRatio = aspectRatio,

				UseBilinearInterpolation = this.UseBilinearInterpolation,
				UseSrgbTextureFormat = this.UseSrgbTextureFormat && this.UseBilinearInterpolation,
				VerticalSync = this.VerticalSync,
				IntegerFpsMode = this.IntegerFpsMode,

				Brightness = this.Brightness / 100.0,
				Contrast = this.Contrast / 100.0,
				Hue = this.Hue / 100.0,
				Saturation = this.Saturation / 100.0,
				ScanlineIntensity = this.ScanlineIntensity / 100.0,

				LcdGridTopLeftBrightness = this.LcdGridTopLeftBrightness / 100.0,
				LcdGridTopRightBrightness = this.LcdGridTopRightBrightness / 100.0,
				LcdGridBottomLeftBrightness = this.LcdGridBottomLeftBrightness / 100.0,
				LcdGridBottomRightBrightness = this.LcdGridBottomRightBrightness / 100.0,

				NtscArtifacts = this.NtscArtifacts / 100.0,
				NtscBleed = this.NtscBleed / 100.0,
				NtscFringing = this.NtscFringing / 100.0,
				NtscGamma = this.NtscGamma / 100.0,
				NtscResolution = this.NtscResolution / 100.0,
				NtscSharpness = this.NtscSharpness / 100.0,
				NtscMergeFields = this.NtscMergeFields,

				NtscScale = this.NtscScale,
				NtscYFilterLength = this.NtscYFilterLength / 100.0,
				NtscIFilterLength = this.NtscIFilterLength / 100.0,
				NtscQFilterLength = this.NtscQFilterLength / 100.0,

				EnableVariableRefreshRate = this.EnableVariableRefreshRate,

				FullscreenForceIntegerScale = this.FullscreenForceIntegerScale,
				UseExclusiveFullscreen = this.UseExclusiveFullscreen,
				ExclusiveFullscreenRefreshRateNtsc = this.ExclusiveFullscreenRefreshRateNtsc,
				ExclusiveFullscreenRefreshRatePal = this.ExclusiveFullscreenRefreshRatePal,

				ScreenRotation = (uint)ScreenRotation,
				DisableHighPrecisionFramePacing = this.DisableHighPrecisionFramePacing
			});
		}

		public UInt32 GetFullscreenWidth()
		{
			uint monitorWidth = (uint)(ApplicationHelper.GetMainWindow()?.Screens.Primary?.Bounds.Width ?? 1920);
			if(UseExclusiveFullscreen) {
				return ExclusiveFullscreenResolution == FullscreenResolution.Default ? monitorWidth : (uint)ExclusiveFullscreenResolution.GetWidth();
			} else {
				return monitorWidth;
			}
		}

		public UInt32 GetFullscreenHeight()
		{
			uint monitorHeight = (uint)(ApplicationHelper.GetMainWindow()?.Screens.Primary?.Bounds.Height ?? 1080);
			if(UseExclusiveFullscreen) {
				return ExclusiveFullscreenResolution == FullscreenResolution.Default ? monitorHeight : (uint)ExclusiveFullscreenResolution.GetHeight();
			} else {
				return monitorHeight;
			}
		}
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct InteropVideoConfig
	{
		public double CustomAspectRatio;
		public VideoFilterType VideoFilter;
		public VideoAspectRatio AspectRatio;

		[MarshalAs(UnmanagedType.I1)] public bool UseBilinearInterpolation;
		[MarshalAs(UnmanagedType.I1)] public bool UseSrgbTextureFormat;
		[MarshalAs(UnmanagedType.I1)] public bool VerticalSync;
		[MarshalAs(UnmanagedType.I1)] public bool IntegerFpsMode;

		public double Brightness;
		public double Contrast;
		public double Hue;
		public double Saturation;
		public double ScanlineIntensity;

		public double LcdGridTopLeftBrightness;
		public double LcdGridTopRightBrightness;
		public double LcdGridBottomLeftBrightness;
		public double LcdGridBottomRightBrightness;

		public double NtscArtifacts;
		public double NtscBleed;
		public double NtscFringing;
		public double NtscGamma;
		public double NtscResolution;
		public double NtscSharpness;
		[MarshalAs(UnmanagedType.I1)] public bool NtscMergeFields;

		public NtscBisqwitFilterScale NtscScale;
		public double NtscYFilterLength;
		public double NtscIFilterLength;
		public double NtscQFilterLength;

		[MarshalAs(UnmanagedType.I1)] public bool EnableVariableRefreshRate;
		[MarshalAs(UnmanagedType.I1)] public bool FullscreenForceIntegerScale;
		[MarshalAs(UnmanagedType.I1)] public bool UseExclusiveFullscreen;
		public UInt32 ExclusiveFullscreenRefreshRateNtsc;
		public UInt32 ExclusiveFullscreenRefreshRatePal;

		public UInt32 ScreenRotation;

		[MarshalAs(UnmanagedType.I1)] public bool DisableHighPrecisionFramePacing;
	}

	public enum VideoFilterType
	{
		None = 0,
		NtscBlargg,
		NtscBisqwit,
		LcdGrid,
		xBRZ2x,
		xBRZ3x,
		xBRZ4x,
		xBRZ5x,
		xBRZ6x,
		HQ2x,
		HQ3x,
		HQ4x,
		Scale2x,
		Scale3x,
		Scale4x,
		_2xSai,
		Super2xSai,
		SuperEagle,
		Prescale2x,
		Prescale3x,
		Prescale4x,
		Prescale6x,
		Prescale8x,
		Prescale10x
	}

	public enum VideoAspectRatio
	{
		NoStretching = 0,
		Auto = 1,
		NTSC = 2,
		PAL = 3,
		Standard = 4,
		Widescreen = 5,
		Custom = 6
	}

	public enum ScreenRotation
	{
		None = 0,
		_90Degrees = 90,
		_180Degrees = 180,
		_270Degrees = 270
	}

	public enum NtscBisqwitFilterScale
	{
		_2x,
		_4x,
		_8x
	}

	public enum FullscreenResolution
	{
		Default,
		_3840x2160,
		_2560x1440,
		_2160x1200,
		_1920x1440,
		_1920x1200,
		_1920x1080,
		_1680x1050,
		_1600x1200,
		_1600x1024,
		_1600x900,
		_1366x768,
		_1360x768,
		_1280x1024,
		_1280x960,
		_1280x800,
		_1280x768,
		_1280x720,
		_1152x864,
		_1024x768,
		_800x600,
		_640x480
	}

	public static class FullscreenResolutionExtensions
	{
		public static int GetWidth(this FullscreenResolution res)
		{
			return Int32.Parse(res.ToString().Substring(1, res.ToString().IndexOf("x") - 1));
		}

		public static int GetHeight(this FullscreenResolution res)
		{
			return Int32.Parse(res.ToString().Substring(res.ToString().IndexOf("x") + 1));
		}
	}
}
