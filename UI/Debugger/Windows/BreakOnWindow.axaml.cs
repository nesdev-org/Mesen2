using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Mesen.Controls;
using Mesen.Interop;
using Mesen.Utilities;
using System;

namespace Mesen.Debugger.Windows
{
	public class BreakOnWindow : MesenWindow
	{
		private static int _lastValue = 0;

		public static readonly StyledProperty<int> ValueProperty = AvaloniaProperty.Register<BreakOnWindow, int>(nameof(Value));
		public static readonly StyledProperty<int?> MinProperty = AvaloniaProperty.Register<BreakOnWindow, int?>(nameof(Min));
		public static readonly StyledProperty<int?> MaxProperty = AvaloniaProperty.Register<BreakOnWindow, int?>(nameof(Max));

		public int Value
		{
			get { return GetValue(ValueProperty); }
			set { SetValue(ValueProperty, value); }
		}

		public int? Min
		{
			get { return GetValue(MinProperty); }
			set { SetValue(MinProperty, value); }
		}

		public int? Max
		{
			get { return GetValue(MaxProperty); }
			set { SetValue(MaxProperty, value); }
		}

		private CpuType _cpuType;

		[Obsolete("For designer only")]
		public BreakOnWindow() : this(CpuType.Snes) { }

		public BreakOnWindow(CpuType cpuType)
		{
			_cpuType = cpuType;
			Value = _lastValue;

			if(!Design.IsDesignMode) {
				TimingInfo timing = EmuApi.GetTimingInfo(_cpuType);
				Min = timing.FirstScanline;
				Max = (int)timing.ScanlineCount + timing.FirstScanline - 1;
			}

			InitializeComponent();
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		protected override void OnOpened(EventArgs e)
		{
			base.OnOpened(e);
			this.GetControl<MesenNumericTextBox>("txtValue").FocusAndSelectAll();
		}

		private void Ok_OnClick(object sender, RoutedEventArgs e)
		{
			_lastValue = Value;
			DebugApi.Step(_cpuType, Value, StepType.SpecificScanline);
			Close();
		}

		private void Cancel_OnClick(object sender, RoutedEventArgs e)
		{
			Close();
		}
	}
}
