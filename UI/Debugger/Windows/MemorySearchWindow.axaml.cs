using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using DataBoxControl;
using Mesen.Config;
using Mesen.Debugger.Controls;
using Mesen.Debugger.Utilities;
using Mesen.Debugger.ViewModels;
using Mesen.Interop;
using Mesen.Utilities;
using Mesen.Windows;
using System;
using System.Collections.Generic;
using System.ComponentModel;

namespace Mesen.Debugger.Windows
{
	public class MemorySearchWindow : MesenWindow, INotificationHandler
	{
		private MemorySearchViewModel _model;

		public MemorySearchWindow()
		{
			_model = new MemorySearchViewModel();
			DataContext = _model;

			InitializeComponent();

			if(Design.IsDesignMode) {
				return;
			}

			_model.Config.LoadWindowSettings(this);
			_model.AddDisposables(DebugShortcutManager.CreateContextMenu(this.GetControl<DataBox>("SearchResults"), new ContextMenuAction[] {
				GetViewInDebuggerAction(),
				GetViewInMemoryAction(),
				GetAddWatchAction(),
				GetAddCheatAction()
			}));
		}

		protected override void OnClosing(WindowClosingEventArgs e)
		{
			base.OnClosing(e);
			_model.Config.SaveWindowSettings(this);
		}

		public void ProcessNotification(NotificationEventArgs e)
		{
			switch(e.NotificationType) {
				case ConsoleNotificationType.GameLoaded:
					Dispatcher.UIThread.Post(() => {
						_model.OnGameLoaded();
					});
					break;

				case ConsoleNotificationType.PpuFrameDone:
					if(!ToolRefreshHelper.LimitFps(this, 30)) {
						_model.RefreshData(false);
					}
					break;

				case ConsoleNotificationType.CodeBreak:
					_model.RefreshData(true);
					break;
			}
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		private ContextMenuAction GetViewInMemoryAction()
		{
			return new ContextMenuAction() {
				ActionType = ActionType.ViewInMemoryViewer,
				IsEnabled = () => _model.GetSelectedAddress() != null,
				OnClick = () => ViewSelectedAddressInMemory()
			};
		}

		private ContextMenuAction GetViewInDebuggerAction()
		{
			return new ContextMenuAction() {
				ActionType = ActionType.ViewInDebugger,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.MemoryViewer_ViewInDebugger),
				IsEnabled = () => _model.GetSelectedRelativeAddress() is AddressInfo address && address.Address >= 0,
				OnClick = () => {
					AddressInfo? address = _model.GetSelectedRelativeAddress();
					if(address?.Address >= 0) {
						DebuggerWindow.OpenWindowAtAddress(address.Value.Type.ToCpuType(), address.Value.Address);
					}
				}
			};
		}

		private ContextMenuAction GetAddWatchAction()
		{
			return new ContextMenuAction() {
				ActionType = ActionType.AddWatch,
				IsEnabled = () => _model.GetSelectedRelativeAddress() is AddressInfo address && address.Type.SupportsWatch(),
				OnClick = () => {
					AddressInfo? address = _model.GetSelectedRelativeAddress();
					if(address != null) {
						CpuType cpuType = _model.MemoryType.ToCpuType();
						string format = "X" + cpuType.GetAddressSize();
						WatchManager.GetWatchManager(cpuType).AddWatch("[$" + address.Value.Address.ToString(format) + "]");
						DebugWindowManager.GetOrOpenDebugWindow(() => new WatchWindow(new WatchWindowViewModel()));
					}
				}
			};
		}

		private ContextMenuAction GetAddCheatAction()
		{
			return new ContextMenuAction() {
				ActionType = ActionType.AddCheat,
				IsEnabled = () => GetCheatCode() != null,
				OnClick = async () => {
					CheatCode? cheat = GetCheatCode();
					if(cheat != null && await CheatEditWindow.EditCheat(cheat, this)) {
						CheatListWindow.AddCheat(this, cheat);
					}
				}
			};
		}

		private void ViewSelectedAddressInMemory()
		{
			int? address = _model.GetSelectedAddress();
			if(address != null) {
				MemoryToolsWindow.ShowInMemoryTools(_model.MemoryType, address.Value);
			}
		}

		private CheatCode? GetCheatCode()
		{
			AddressInfo? address = _model.GetSelectedRelativeAddress();
			uint? rawValue = _model.GetSelectedRawValue();
			if(address == null || rawValue == null) {
				return null;
			}

			CheatType? cheatType = _model.MemoryType.ToCpuType() switch {
				CpuType.Nes => CheatType.NesCustom,
				CpuType.Snes => CheatType.SnesProActionReplay,
				CpuType.Gameboy => CheatType.GbGameShark,
				CpuType.Pce => CheatType.PceRaw,
				CpuType.Sms => CheatType.SmsProActionReplay,
				_ => null
			};
			if(cheatType == null) {
				return null;
			}

			uint codeAddress = (uint)address.Value.Address;
			List<string> codes = new();
			for(int i = 0; i < (int)_model.ValueSize; i++) {
				byte value = (byte)(rawValue.Value >> (i * 8));
				string code = cheatType.Value switch {
					CheatType.NesCustom => $"{codeAddress:X4}:{value:X2}",
					CheatType.SnesProActionReplay => $"{codeAddress:X6}{value:X2}",
					CheatType.GbGameShark => $"01{value:X2}{(codeAddress & 0xFF):X2}{((codeAddress >> 8) & 0xFF):X2}",
					CheatType.PceRaw => $"{codeAddress:X6}:{value:X2}",
					CheatType.SmsProActionReplay => $"{codeAddress:X4}{value:X2}00",
					_ => ""
				};

				InteropInternalCheatCode convertedCode = new();
				if(string.IsNullOrEmpty(code) || !EmuApi.GetConvertedCheat(new InteropCheatCode(cheatType.Value, code), ref convertedCode)) {
					return null;
				}
				codes.Add(code);
				codeAddress++;
			}

			return new CheatCode() {
				Description = "Memory Search - $" + _model.GetSelectedAddress()!.Value.ToString("X4"),
				Type = cheatType.Value,
				Enabled = false,
				Codes = string.Join(Environment.NewLine, codes)
			};
		}

		private void OnCellDoubleClick(DataBoxCell cell)
		{
			ViewSelectedAddressInMemory();
		}
	}
}
