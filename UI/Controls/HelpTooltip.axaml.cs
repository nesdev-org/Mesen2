using Avalonia;
using Avalonia.Controls;
using Avalonia.Markup.Xaml;
using System;

namespace Mesen.Controls;

public class HelpTooltip : UserControl
{
	public static readonly StyledProperty<string> TextProperty = AvaloniaProperty.Register<HelpTooltip, string>(nameof(Text));
	public static readonly StyledProperty<bool> IsWarningProperty = AvaloniaProperty.Register<HelpTooltip, bool>(nameof(IsWarningProperty));

	public string Text
	{
		get { return GetValue(TextProperty); }
		set { SetValue(TextProperty, value); }
	}

	public bool IsWarning
	{
		get { return GetValue(IsWarningProperty); }
		set { SetValue(IsWarningProperty, value); }
	}

	public HelpTooltip()
	{
		InitializeComponent();
	}

	private void InitializeComponent()
	{
		AvaloniaXamlLoader.Load(this);
	}
}
