using Avalonia;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Config;
using Mesen.Controls;
using Mesen.Interop;
using Mesen.Localization;
using Mesen.Utilities;
using Mesen.Windows;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Mesen.ViewModels
{
	public partial class MainWindowViewModel : DisposableViewModel
	{
		public static MainWindowViewModel Instance { get; private set; } = null!;

		[ObservableProperty] public partial MainMenuViewModel MainMenu { get; set; }
		[ObservableProperty] public partial RomInfo RomInfo { get; set; }
		[ObservableProperty] public partial AudioPlayerViewModel? AudioPlayer { get; private set; }
		[ObservableProperty] public partial RecentGamesViewModel RecentGames { get; private set; }

		[ObservableProperty] public partial string WindowTitle { get; private set; } = "MesenCE";
		[ObservableProperty] public partial Size RendererSize { get; set; }

		[ObservableProperty] public partial bool IsMenuVisible { get; set; }

		[ObservableProperty] public partial bool IsNativeRendererVisible { get; private set; }
		[ObservableProperty] public partial bool IsSoftwareRendererVisible { get; private set; }

		public SoftwareRendererViewModel SoftwareRenderer { get; } = new();

		public Configuration Config { get; }
		public NativeRenderer? Renderer { get; internal set; }

		public MainWindowViewModel()
		{
			Instance = this;

			Config = ConfigManager.Config;
			MainMenu = new MainMenuViewModel(this);
			RomInfo = new RomInfo();
			RecentGames = new RecentGamesViewModel();

			IsMenuVisible = !Config.Preferences.AutoHideMenu;
		}

		protected override void DisposeView()
		{
			base.DisposeView();
			MainMenu.Dispose();
		}

		public void Init(MainWindow wnd)
		{
			MainMenu.Initialize(wnd);
			RecentGames.Init(GameScreenMode.RecentGames);

			AddDisposable(RecentGames.ObserveProp(nameof(RecentGamesViewModel.Visible), () => {
				UpdateRendererVisibility();
			}));

			AddDisposable(SoftwareRenderer.ObserveProp(nameof(SoftwareRendererViewModel.FrameSurface), () => {
				UpdateRendererVisibility();
			}));

			AddDisposable(this.ObserveProp(nameof(MainWindowViewModel.RendererSize), UpdateWindowTitle));

			AddDisposable(ReactiveHelper.RegisterForeignObserver([(() => Config, nameof(Configuration.Video)), (() => Config.Video, nameof(VideoConfig.AspectRatio))], UpdateWindowTitle));
			AddDisposable(ReactiveHelper.RegisterForeignObserver([(() => Config, nameof(Configuration.Video)), (() => Config.Video, nameof(VideoConfig.VideoFilter))], UpdateWindowTitle));
			AddDisposable(ReactiveHelper.RegisterForeignObserver([(() => Config, nameof(Configuration.Preferences)), (() => Config.Preferences, nameof(PreferencesConfig.ShowTitleBarInfo))], UpdateWindowTitle));

			UpdateWindowTitle();
		}

		private void UpdateRendererVisibility()
		{
			IsNativeRendererVisible = !RecentGames.Visible && SoftwareRenderer.FrameSurface == null;
			IsSoftwareRendererVisible = !RecentGames.Visible && SoftwareRenderer.FrameSurface != null;

			if(Renderer != null) {
				Dispatcher.UIThread.Post(() => {
					Renderer.IsVisible = IsNativeRendererVisible;
				});
			}
		}

		partial void OnRomInfoChanged(RomInfo value)
		{
			bool showAudioPlayer = RomInfo.Format == RomFormat.Nsf || RomInfo.Format == RomFormat.Spc || RomInfo.Format == RomFormat.Gbs || RomInfo.Format == RomFormat.PceHes;
			AudioPlayer?.Dispose();
			if(AudioPlayer == null && showAudioPlayer) {
				AudioPlayer = new AudioPlayerViewModel();
			} else if(!showAudioPlayer) {
				AudioPlayer = null;
			}

			UpdateWindowTitle();
		}

		private void UpdateWindowTitle()
		{
			string title = "MesenCE";
			string romName = RomInfo.GetRomName();
			if(!string.IsNullOrWhiteSpace(romName)) {
				title += " - " + romName;
				if(ConfigManager.Config.Preferences.ShowTitleBarInfo) {
					FrameInfo baseSize = EmuApi.GetBaseScreenSize();
					double scale = (double)RendererSize.Height / baseSize.Height;
					title += string.Format(" - {0}x{1} ({2:0.###}x, {3})",
						Math.Round(RendererSize.Width),
						Math.Round(RendererSize.Height),
						scale,
						ResourceHelper.GetEnumText(ConfigManager.Config.Video.VideoFilter));
				}
			}
			WindowTitle = title;
		}
	}
}
