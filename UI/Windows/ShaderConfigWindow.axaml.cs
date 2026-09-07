using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Mesen.Config;
using Mesen.ViewModels;
using System;
using System.IO;

namespace Mesen.Windows;

public class ShaderConfigWindow : MesenWindow
{
	public ShaderConfigWindow()
	{
		InitializeComponent();
	}

	private void InitializeComponent()
	{
		AvaloniaXamlLoader.Load(this);
	}

	protected override void OnDataContextChanged(EventArgs e)
	{
		base.OnDataContextChanged(e);
		if(DataContext is ShaderConfigViewModel model) {
			Title += ": " + Path.GetFileNameWithoutExtension(model.Config.ShaderFile);
		}
	}

	protected override void OnClosed(EventArgs e)
	{
		base.OnClosed(e);
		ConfigManager.Config.Video.ApplyConfig();
	}

	private void Ok_OnClick(object sender, RoutedEventArgs e)
	{
		(DataContext as ShaderConfigViewModel)?.Save();
		Close();
	}

	private void Reset_OnClick(object sender, RoutedEventArgs e)
	{
		(DataContext as ShaderConfigViewModel)?.ResetToDefaults();
	}

	private void Cancel_OnClick(object sender, RoutedEventArgs e)
	{
		Close();
	}
}
