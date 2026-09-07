using Avalonia;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Interop;
using Mesen.Utilities;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Mesen.Config;

public partial class ShaderConfig : BaseConfig<GameConfig>
{
	public string ShaderFile { get; set; } = "";
	[ObservableProperty] public partial List<ShaderParam> Params { get; set; } = new();

	public void ApplyConfig()
	{
		InteropShaderParamValue[] shaderParams = Params.Select(p => {
			InteropShaderParamValue paramValue = new InteropShaderParamValue();

			byte[] name = Encoding.UTF8.GetBytes(p.Name);
			unsafe {
				fixed(byte* src = name) {
					Buffer.MemoryCopy(src, paramValue.Name, 200, name.Length);
				}
			}
			paramValue.Value = (double)p.Value;
			return paramValue;
		}).ToArray();

		ShaderConfigHelper.ApplyConfig(ShaderFile, shaderParams);
	}

	public static ShaderConfig LoadConfig(string shaderFile)
	{
		if(string.IsNullOrWhiteSpace(shaderFile) || !File.Exists(shaderFile)) {
			return new();
		}

		InteropShaderParam[] shaderParams = ConfigApi.GetShaderParams(shaderFile);

		string path = Path.Combine(ConfigManager.ShaderConfigFolder, Path.GetFileNameWithoutExtension(shaderFile) + ".json");
		ShaderConfig? cfg = null;
		if(File.Exists(path)) {
			string? fileData = FileHelper.ReadAllText(path);
			if(fileData != null) {
				try {
					cfg = (ShaderConfig?)JsonSerializer.Deserialize(fileData, typeof(ShaderConfig), MesenSerializerContext.Default);
				} catch { }
			}
		}

		if(cfg == null) {
			cfg = new ShaderConfig();
		}

		bool updateParams = cfg.Params.Count != shaderParams.Length;
		if(!updateParams) {
			for(int i = 0; i < shaderParams.Length; i++) {
				if(cfg.Params[i].Name != shaderParams[i].GetName()) {
					updateParams = true;
					break;
				}
			}
		}

		if(updateParams) {
			cfg.Params = shaderParams.Select(p => new ShaderParam() {
				Name = p.GetName(),
				Value = ToDecimal(p.Initial)
			}).ToList();
		}

		for(int i = 0; i < shaderParams.Length; i++) {
			cfg.Params[i].Description = shaderParams[i].GetDescription().Trim();
			cfg.Params[i].Min = ToDecimal(shaderParams[i].Min);
			cfg.Params[i].Max = ToDecimal(shaderParams[i].Max);
			cfg.Params[i].Step = ToDecimal(shaderParams[i].Step);
			cfg.Params[i].Initial = ToDecimal(shaderParams[i].Initial);
		}

		cfg.ShaderFile = shaderFile;
		return cfg;
	}

	public void Save(string shaderFile)
	{
		string path = Path.Combine(ConfigManager.ShaderConfigFolder, Path.GetFileNameWithoutExtension(shaderFile) + ".json");
		ShaderFile = shaderFile;
		FileHelper.WriteAllText(path, JsonSerializer.Serialize(this, typeof(ShaderConfig), MesenSerializerContext.Default));
	}

	internal void ResetToDefaults()
	{
		foreach(ShaderParam p in Params) {
			p.Value = (decimal)p.Initial;
		}
	}

	private static decimal ToDecimal(double v)
	{
		return Math.Round((decimal)v * 10000) / 10000;
	}
}

public partial class ShaderParam : ObservableObject
{
	public string Name { get; set; } = "";

	[NotifyPropertyChangedFor(nameof(BoolValue))]
	[ObservableProperty] public partial decimal Value { get; set; } = 0;

	[JsonIgnore] public string Description { get; set; } = "";
	[JsonIgnore] public decimal Min { get; set; } = 0;
	[JsonIgnore] public decimal Max { get; set; } = 0;
	[JsonIgnore] public decimal Step { get; set; } = 0;
	[JsonIgnore] public decimal Initial { get; set; } = 0;

	[JsonIgnore]
	public bool IsBoolean => Min == 0 && Max == 1 && Step == 1;

	[JsonIgnore]
	public bool IsLabel
	{
		get
		{
			return (
				Min == Max ||
				(Min == 0 && Max == Step && Step <= 0.001m) ||
				string.IsNullOrWhiteSpace(Description)
			);
		}
	}

	[JsonIgnore]
	public bool BoolValue
	{
		get => Value != 0;
		set => Value = value ? 1 : 0;
	}

	[JsonIgnore]
	public string FormatString
	{
		get
		{
			string val = (Math.Round(Step * 100000) / 100000).ToString(CultureInfo.InvariantCulture);
			int index = val.IndexOf('.');
			if(index >= 0) {
				int decimalCount = val.Length - (index + 1);
				return "0." + new string('0', decimalCount);
			} else {
				return "0";
			}
		}
	}
}

public static class ShaderConfigHelper
{
	private static UInt32 _version = 0;
	private static string _prevShaderFile = "";
	private static InteropShaderParamValue[] _prevParamValues = Array.Empty<InteropShaderParamValue>();

	public static void ApplyConfig(string shaderFile, InteropShaderParamValue[] paramValues)
	{
		bool needUpdate = _prevShaderFile != shaderFile || _prevParamValues.Length != paramValues.Length;

		if(!needUpdate) {
			for(int i = 0; i < paramValues.Length; i++) {
				if(paramValues[i].Value != _prevParamValues[i].Value || paramValues[i].GetName() != _prevParamValues[i].GetName()) {
					needUpdate = true;
					break;
				}
			}
		}

		if(needUpdate) {
			_prevParamValues = paramValues;
			_prevShaderFile = shaderFile;

			unsafe {
				fixed(InteropShaderParamValue* ptr = paramValues) {
					ConfigApi.SetShaderConfig(new InteropShaderConfig() {
						ConfigVersion = ++_version,
						ShaderFile = shaderFile,
						Params = (IntPtr)ptr,
						ParamCount = (UInt32)paramValues.Length
					});
				}
			}
		}
	}
}
