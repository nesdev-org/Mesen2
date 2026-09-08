using Avalonia;
using Avalonia.Controls;
using System;

namespace Mesen.Controls;

public class MesenNumericUpDown : NumericUpDown
{
	public static readonly StyledProperty<bool> ForceIncrementMultipleProperty = AvaloniaProperty.Register<MesenNumericUpDown, bool>(nameof(ForceIncrementMultiple), false);
	protected override Type StyleKeyOverride => typeof(NumericUpDown);

	public bool ForceIncrementMultiple
	{
		get { return GetValue(ForceIncrementMultipleProperty); }
		set { SetValue(ForceIncrementMultipleProperty, value); }
	}

	protected override void OnTextChanged(string? oldValue, string? newValue)
	{
		if(newValue == null || newValue == "") {
			//Prevent displaying invalid cast error/breaking the layout when user clears the content of the control
			Text = "0";
		}
		base.OnTextChanged(oldValue, newValue);
	}

	protected override decimal? OnCoerceValue(decimal? baseValue)
	{
		if(baseValue == null || !ForceIncrementMultiple) {
			return base.OnCoerceValue(baseValue);
		}
		return Math.Round(baseValue.Value / Increment) * Increment;
	}
}
