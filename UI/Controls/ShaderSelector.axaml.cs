using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Mesen.Config;
using Mesen.Interop;
using Mesen.Localization;
using Mesen.Utilities;
using System;
using System.Collections.Generic;
using System.IO;

namespace Mesen.Controls;

public class ShaderSelector : UserControl
{
	public static readonly StyledProperty<string> ShaderFileProperty = AvaloniaProperty.Register<ShaderSelector, string>(nameof(ShaderFile), defaultBindingMode: Avalonia.Data.BindingMode.TwoWay);
	public static readonly StyledProperty<string> DisplayValueProperty = AvaloniaProperty.Register<ShaderSelector, string>(nameof(DisplayValue));
	public static readonly StyledProperty<ConsoleOverrideConfig> ConsoleOverrideConfigProperty = AvaloniaProperty.Register<ShaderSelector, ConsoleOverrideConfig>(nameof(ConsoleOverrideConfig));

	public string ShaderFile
	{
		get { return GetValue(ShaderFileProperty); }
		set { SetValue(ShaderFileProperty, value); }
	}

	public string DisplayValue
	{
		get { return GetValue(DisplayValueProperty); }
		set { SetValue(DisplayValueProperty, value); }
	}

	public ConsoleOverrideConfig ConsoleOverrideConfig
	{
		get { return GetValue(ConsoleOverrideConfigProperty); }
		set { SetValue(ConsoleOverrideConfigProperty, value); }
	}

	static ShaderSelector()
	{
		ShaderFileProperty.Changed.AddClassHandler<ShaderSelector>((x, e) => {
			if(string.IsNullOrWhiteSpace(x.ShaderFile)) {
				x.DisplayValue = ResourceHelper.GetMessage("None");
			} else {
				x.DisplayValue = Path.GetFileNameWithoutExtension(x.ShaderFile);
			}
		});
	}

	public ShaderSelector()
	{
		InitializeComponent();
	}

	private void InitializeComponent()
	{
		AvaloniaXamlLoader.Load(this);
	}

	private async void BtnSettings_OnClick(object sender, RoutedEventArgs e)
	{
		Window? wnd = this.GetWindow();
		if(wnd == null) {
			return;
		}

		Control ctrl = (Control)sender;
		//Make sure the previous menu is closed before creating a new one
		ctrl.ContextMenu = null;

		//Create the menu on each click to make sure newly added shaders appear in the list
		ctrl.ContextMenu = new ContextMenu() {
			Name = "ActionMenu",
			Placement = PlacementMode.BottomEdgeAlignedLeft,
			ItemsSource = ShaderMenuHelper.GetShaderMenu(wnd, () => ShaderFile, v => ShaderFile = v, ConsoleOverrideConfig == null || ConsoleOverrideConfig.GetActiveOverride() == ConsoleOverrideConfig).SubActions
		};
		ctrl.ContextMenu.Open();

		ctrl.ContextMenu.Closed += (s, e) => {
			if(s is ContextMenu ctx && ctx.ItemsSource != null) {
				foreach(object item in ctx.ItemsSource) {
					if(item is IDisposable d) {
						d.Dispose();
					}
				}
			}
			ctrl.ContextMenu = null;
		};
	}
}
