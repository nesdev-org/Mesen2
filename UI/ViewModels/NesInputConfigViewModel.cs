using Avalonia;
using Avalonia.Controls;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Config;
using Mesen.Config.Shortcuts;
using Mesen.Interop;
using Mesen.Localization;
using Mesen.Utilities;
using Mesen.Views;
using Mesen.Windows;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Mesen.ViewModels
{
	public partial class NesInputConfigViewModel : DisposableViewModel
	{
		[ObservableProperty] public partial NesConfig Config { get; set; }

		public List<ShortcutKeyInfo> ShortcutKeys { get; set; }

		private MainWindowViewModel MainWindow { get; }

		[ObservableProperty] public partial bool ShowMapperInput { get; private set; }
		[ObservableProperty] public partial bool HasFourScore { get; private set; }

		[ObservableProperty] public partial bool HasFourPlayerAdapter { get; private set; }
		[ObservableProperty] public partial bool HasExpansionHub { get; private set; }
		[ObservableProperty] public partial string ExpConfigLabel { get; private set; } = "";
		[ObservableProperty] public partial Enum[] AvailableControllerTypesExpansionHub { get; private set; } = Array.Empty<Enum>();

		public Enum[] AvailableControllerTypesP1 => new Enum[] {
			ControllerType.None,
			ControllerType.NesController,
			ControllerType.FamicomController,
			ControllerType.NesZapper,
			ControllerType.FourScore,
			ControllerType.NesArkanoidController,
			ControllerType.PowerPadSideA,
			ControllerType.PowerPadSideB,
			ControllerType.SnesController,
			ControllerType.SnesMouse,
			ControllerType.SnesNttDataKeypad,
			ControllerType.SuborMouse,
			ControllerType.VbController
		};

		public Enum[] AvailableControllerTypesP2 => new Enum[] {
			ControllerType.None,
			ControllerType.NesController,
			ControllerType.FamicomControllerP2,
			ControllerType.NesZapper,
			ControllerType.FourScore,
			ControllerType.NesArkanoidController,
			ControllerType.PowerPadSideA,
			ControllerType.PowerPadSideB,
			ControllerType.SnesController,
			ControllerType.SnesMouse,
			ControllerType.SnesNttDataKeypad,
			ControllerType.SuborMouse,
			ControllerType.VbController
		};

		public Enum[] AvailableControllerTypesFourPlayer => new Enum[] {
			ControllerType.None,
			ControllerType.NesController,
		};

		public Enum[] AvailableControllerTypesTwoPlayer => new Enum[] {
			ControllerType.None,
			ControllerType.NesController,
			ControllerType.Pachinko,
			ControllerType.FcnsController,
			ControllerType.SnesController,
			ControllerType.SnesMouse,
			ControllerType.SnesNttDataKeypad,
			ControllerType.SuborMouse,
			ControllerType.VbController,
		};

		public Enum[] AvailableExpansionTypes => new Enum[] {
			ControllerType.None,
			ControllerType.FamicomZapper,
			ControllerType.TwoPlayerAdapter,
			ControllerType.FourPlayerAdapter,
			ControllerType.FamicomArkanoidController,
			ControllerType.OekaKidsTablet,
			ControllerType.FamilyTrainerMatSideA,
			ControllerType.FamilyTrainerMatSideB,
			ControllerType.KonamiHyperShot,
			ControllerType.FamilyBasicKeyboard,
			ControllerType.PartyTap,
			ControllerType.Pachinko,
			ControllerType.FcnsController,
			ControllerType.ExcitingBoxing,
			ControllerType.JissenMahjong,
			ControllerType.SuborKeyboard,
			ControllerType.BarcodeBattler,
			ControllerType.HoriTrack,
			ControllerType.BandaiHyperShot,
			ControllerType.AsciiTurboFile,
			ControllerType.BattleBox
		};

		public Enum[] AvailableControllerTypesMapperInput => new Enum[] {
			ControllerType.BandaiMicrophone,
		};

		[Obsolete("For designer only")]
		public NesInputConfigViewModel() : this(new NesConfig(), new PreferencesConfig()) { }

		public NesInputConfigViewModel(NesConfig config, PreferencesConfig preferences)
		{
			Config = config;
			MainWindow = MainWindowViewModel.Instance;

			ShortcutKeys = new List<ShortcutKeyInfo>();

			if(Design.IsDesignMode) {
				return;
			}

			AddDisposable(ReactiveHelper.RegisterObserver([Config.Port1, Config.Port2], nameof(ControllerConfig.Type), () => {
				HasFourScore = Config.Port1.Type == ControllerType.FourScore || Config.Port2.Type == ControllerType.FourScore;
				if(HasFourScore) {
					Config.Port1.Type = ControllerType.FourScore;
					Config.Port2.Type = ControllerType.None;
				}
			}));

			AddDisposable(Config.ExpPort.ObserveProp(nameof(ControllerConfig.Type), () => {
				ControllerType t = Config.ExpPort.Type;
				HasFourPlayerAdapter = t == ControllerType.FourPlayerAdapter;
				HasExpansionHub = t == ControllerType.TwoPlayerAdapter || t == ControllerType.FourPlayerAdapter;
				ExpConfigLabel = ResourceHelper.GetViewLabel(nameof(NesInputConfigView), t == ControllerType.TwoPlayerAdapter ? "lblTwoPlayerAdapterConfig" : "lblFourPlayerAdapterConfig");
				AvailableControllerTypesExpansionHub = t == ControllerType.TwoPlayerAdapter ? AvailableControllerTypesTwoPlayer : AvailableControllerTypesFourPlayer;
			}));

			AddDisposable(MainWindow.ObserveProp(nameof(MainWindowViewModel.RomInfo), () => {
				ShowMapperInput = InputApi.HasControlDevice(ControllerType.BandaiMicrophone);
			}));

			EmulatorShortcut[] displayOrder = new EmulatorShortcut[] {
				EmulatorShortcut.FdsSwitchDiskSide,
				EmulatorShortcut.FdsEjectDisk,
				EmulatorShortcut.FdsInsertNextDisk,
				EmulatorShortcut.VsInsertCoin1,
				EmulatorShortcut.VsInsertCoin2,
				EmulatorShortcut.VsInsertCoin3,
				EmulatorShortcut.VsInsertCoin4,
				EmulatorShortcut.VsServiceButton,
				EmulatorShortcut.VsServiceButton2
			};

			Dictionary<EmulatorShortcut, ShortcutKeyInfo> shortcuts = new Dictionary<EmulatorShortcut, ShortcutKeyInfo>();
			foreach(ShortcutKeyInfo shortcut in preferences.ShortcutKeys) {
				shortcuts[shortcut.Shortcut] = shortcut;
			}

			for(int i = 0; i < displayOrder.Length; i++) {
				ShortcutKeys.Add(shortcuts[displayOrder[i]]);
			}
		}
	}
}
