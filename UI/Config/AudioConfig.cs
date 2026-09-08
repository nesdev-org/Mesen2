using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Interop;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Mesen.Config
{
	public partial class AudioConfig : BaseConfig<AudioConfig>
	{
		[ObservableProperty] public partial AudioBackendType AudioBackend { get; set; } = AudioBackendType.Default;
		[ObservableProperty] public partial string AudioDevice { get; set; } = "";
		[ObservableProperty] public partial bool EnableAudio { get; set; } = true;
		[ObservableProperty] public partial bool DisableDynamicSampleRate { get; set; } = false;

		[ObservableProperty][MinMax(0, 100)] public partial UInt32 MasterVolume { get; set; } = 100;
		[ObservableProperty] public partial AudioSampleRate SampleRate { get; set; } = AudioSampleRate._48000;
		[ObservableProperty][MinMax(15, 300)] public partial UInt32 AudioLatency { get; set; } = 30;

		[ObservableProperty] public partial bool MuteSoundInBackground { get; set; } = false;
		[ObservableProperty] public partial bool ReduceSoundInBackground { get; set; } = true;
		[ObservableProperty] public partial bool ReduceSoundInFastForward { get; set; } = false;
		[ObservableProperty][MinMax(0, 100)] public partial int VolumeReduction { get; set; } = 75;

		[ObservableProperty] public partial bool ReverbEnabled { get; set; } = false;
		[ObservableProperty][MinMax(1, 10)] public partial UInt32 ReverbStrength { get; set; } = 5;
		[ObservableProperty][MinMax(1, 30)] public partial UInt32 ReverbDelay { get; set; } = 10;

		[ObservableProperty] public partial bool CrossFeedEnabled { get; set; } = false;
		[ObservableProperty][MinMax(0, 100)] public partial UInt32 CrossFeedRatio { get; set; } = 0;

		[ObservableProperty] public partial bool EnableEqualizer { get; set; } = false;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band1Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band2Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band3Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band4Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band5Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band6Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band7Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band8Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band9Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band10Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band11Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band12Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band13Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band14Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band15Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band16Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band17Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band18Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band19Gain { get; set; } = 0;
		[ObservableProperty][MinMax(-20.0, 20.0)] public partial double Band20Gain { get; set; } = 0;

		[ObservableProperty] public partial bool AudioPlayerEnableTrackLength { get; set; } = true;
		[ObservableProperty][MinMax(0, 9999)] public partial UInt32 AudioPlayerTrackLength { get; set; } = 120;
		[ObservableProperty] public partial bool AudioPlayerAutoDetectSilence { get; set; } = true;
		[ObservableProperty][MinMax(0, 999999)] public partial UInt32 AudioPlayerSilenceDelay { get; set; } = 3;

		public void ApplyConfig()
		{
			ConfigApi.SetAudioConfig(new InteropAudioConfig() {
				AudioBackend = AudioBackend,
				AudioDevice = AudioDevice,
				EnableAudio = EnableAudio,
				DisableDynamicSampleRate = DisableDynamicSampleRate,

				MasterVolume = MasterVolume,
				SampleRate = (UInt32)SampleRate,
				AudioLatency = AudioLatency,

				MuteSoundInBackground = MuteSoundInBackground,
				ReduceSoundInBackground = ReduceSoundInBackground,
				ReduceSoundInFastForward = ReduceSoundInFastForward,
				VolumeReduction = VolumeReduction,

				ReverbEnabled = ReverbEnabled,
				ReverbStrength = ReverbStrength,
				ReverbDelay = ReverbDelay,
				CrossFeedEnabled = CrossFeedEnabled,
				CrossFeedRatio = CrossFeedRatio,

				EnableEqualizer = EnableEqualizer,
				Band1Gain = Band1Gain,
				Band2Gain = Band2Gain,
				Band3Gain = Band3Gain,
				Band4Gain = Band4Gain,
				Band5Gain = Band5Gain,
				Band6Gain = Band6Gain,
				Band7Gain = Band7Gain,
				Band8Gain = Band8Gain,
				Band9Gain = Band9Gain,
				Band10Gain = Band10Gain,
				Band11Gain = Band11Gain,
				Band12Gain = Band12Gain,
				Band13Gain = Band13Gain,
				Band14Gain = Band14Gain,
				Band15Gain = Band15Gain,
				Band16Gain = Band16Gain,
				Band17Gain = Band17Gain,
				Band18Gain = Band18Gain,
				Band19Gain = Band19Gain,
				Band20Gain = Band20Gain,

				AudioPlayerEnableTrackLength = AudioPlayerEnableTrackLength,
				AudioPlayerTrackLength = AudioPlayerTrackLength,
				AudioPlayerAutoDetectSilence = AudioPlayerAutoDetectSilence,
				AudioPlayerSilenceDelay = AudioPlayerSilenceDelay
			});
		}
	}

	[StructLayout(LayoutKind.Sequential)]
	public struct InteropAudioConfig
	{
		public AudioBackendType AudioBackend;
		[MarshalAs(UnmanagedType.LPStr)] public string AudioDevice;
		[MarshalAs(UnmanagedType.I1)] public bool EnableAudio;
		[MarshalAs(UnmanagedType.I1)] public bool DisableDynamicSampleRate;

		public UInt32 MasterVolume;
		public UInt32 SampleRate;
		public UInt32 AudioLatency;

		[MarshalAs(UnmanagedType.I1)] public bool MuteSoundInBackground;
		[MarshalAs(UnmanagedType.I1)] public bool ReduceSoundInBackground;
		[MarshalAs(UnmanagedType.I1)] public bool ReduceSoundInFastForward;
		public int VolumeReduction;

		[MarshalAs(UnmanagedType.I1)] public bool ReverbEnabled;
		public UInt32 ReverbStrength;
		public UInt32 ReverbDelay;

		[MarshalAs(UnmanagedType.I1)] public bool CrossFeedEnabled;
		public UInt32 CrossFeedRatio;

		[MarshalAs(UnmanagedType.I1)] public bool EnableEqualizer;
		public double Band1Gain;
		public double Band2Gain;
		public double Band3Gain;
		public double Band4Gain;
		public double Band5Gain;
		public double Band6Gain;
		public double Band7Gain;
		public double Band8Gain;
		public double Band9Gain;
		public double Band10Gain;
		public double Band11Gain;
		public double Band12Gain;
		public double Band13Gain;
		public double Band14Gain;
		public double Band15Gain;
		public double Band16Gain;
		public double Band17Gain;
		public double Band18Gain;
		public double Band19Gain;
		public double Band20Gain;

		[MarshalAs(UnmanagedType.I1)] public bool AudioPlayerEnableTrackLength;
		public UInt32 AudioPlayerTrackLength;
		[MarshalAs(UnmanagedType.I1)] public bool AudioPlayerAutoDetectSilence;
		public UInt32 AudioPlayerSilenceDelay;
	}

	public enum AudioSampleRate
	{
		_11025 = 11025,
		_22050 = 22050,
		_32000 = 32000,
		_44100 = 44100,
		_48000 = 48000,
		_96000 = 96000
	}

	public enum AudioBackendType
	{
		Default,
		Wasapi,
		DirectSound,
		Sdl2
	}
}
