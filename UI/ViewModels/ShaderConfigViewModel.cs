using Avalonia.Controls;
using Mesen.Config;
using Mesen.Interop;
using Mesen.Utilities;
using System;

namespace Mesen.ViewModels;

public partial class ShaderConfigViewModel : DisposableViewModel
{
	public ShaderConfig Config { get; init; }

	private string _shaderFile = "";

	[Obsolete("For designer only")]
	public ShaderConfigViewModel() : this(false, "") { }

	public ShaderConfigViewModel(bool allowPreview, string shaderFile)
	{
		if(Design.IsDesignMode) {
			Config = new();
			return;
		}

		_shaderFile = shaderFile;
		Config = ShaderConfig.LoadConfig(shaderFile);

		if(allowPreview) {
			AddDisposable(ReactiveHelper.RegisterRecursiveObserver(Config, (s, e) => Config.ApplyConfig()));
		}
	}

	public void ResetToDefaults()
	{
		Config.ResetToDefaults();
	}

	public void Save()
	{
		Config.Save(_shaderFile);
	}
}
