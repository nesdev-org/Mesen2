using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Data.Converters;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using DataBoxControl;
using Mesen.Config;
using Mesen.Interop;
using Mesen.Utilities;
using Mesen.ViewModels;
using System;
using System.ComponentModel;
using System.Linq;

namespace Mesen.Windows
{
	public class CheatListWindow : MesenWindow
	{
		private CheatListWindowViewModel _model;
		private NotificationListener _listener;

		public CheatListWindow()
		{
			InitializeComponent();

			_listener = new NotificationListener();
			_listener.OnNotification += OnNotification;

			_model = new CheatListWindowViewModel();
			_model.InitActions(this);
			DataContext = _model;
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		public static void AddCheat(Control parent, CheatCode cheat)
		{
			CheatListWindow wnd = ApplicationHelper.GetOrCreateUniqueWindow(parent, () => new CheatListWindow());
			wnd._model.AddCheat(cheat);
			wnd.BringToFront();
		}

		protected override void OnOpened(EventArgs e)
		{
			base.OnOpened(e);
			ConfigManager.Config.Cheats.LoadWindowSettings(this);
		}

		protected override void OnClosing(WindowClosingEventArgs e)
		{
			_listener.Dispose();
			ConfigManager.Config.Cheats.SaveWindowSettings(this);
			CheatCodes.ApplyCheats();
			base.OnClosing(e);
		}

		private void Ok_OnClick(object sender, RoutedEventArgs e)
		{
			_model.SaveCheats();
			Close();
		}

		private void Cancel_OnClick(object sender, RoutedEventArgs e)
		{
			Close();
		}

		private void OnCellClick(DataBoxCell cell)
		{
			if(cell.DataContext is CheatCode && cell.Column?.ColumnName == "Enabled") {
				bool newValue = !_model.Selection.SelectedItems.Any(cheat => cheat?.Enabled == true);
				foreach(CheatCode? cheat in _model.Selection.SelectedItems) {
					if(cheat != null) {
						cheat.Enabled = newValue;
					}
				}
				_model.Sort();
				_model.ApplyCheats();
			}
		}

		private async void OnCellDoubleClick(DataBoxCell cell)
		{
			if(cell.DataContext is CheatCode cheat) {
				await CheatEditWindow.EditCheat(cheat, this);
				_model.Sort();
				_model.ApplyCheats();
			}
		}

		public void OnNotification(NotificationEventArgs e)
		{
			switch(e.NotificationType) {
				case ConsoleNotificationType.BeforeGameLoad:
				case ConsoleNotificationType.BeforeGameUnload:
					_model.SaveCheats();
					break;

				case ConsoleNotificationType.GameLoaded:
					Dispatcher.UIThread.Post(() => {
						_model.LoadCheats();
					});
					break;

				case ConsoleNotificationType.BeforeEmulationStop:
					_model.SaveCheats();
					Dispatcher.UIThread.Post(() => {
						Close();
					});
					break;
			}
		}
	}

	public class CodeStringConverter : IValueConverter
	{
		public object Convert(object? value, Type targetType, object? parameter, System.Globalization.CultureInfo culture)
		{
			if(targetType == typeof(string) && value is string str) {
				return string.Join(", ", str.Split(Environment.NewLine, StringSplitOptions.RemoveEmptyEntries));
			}
			throw new NotSupportedException();
		}

		public object ConvertBack(object? value, Type targetType, object? parameter, System.Globalization.CultureInfo culture)
		{
			throw new NotSupportedException();
		}
	}
}
