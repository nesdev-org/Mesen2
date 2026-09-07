using Avalonia.Controls;
using Mesen.Config;
using Mesen.Debugger.Utilities;
using Mesen.Interop;
using Mesen.ViewModels;
using Mesen.Windows;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Mesen.Utilities;

public static class ShaderMenuHelper
{
	public static MainMenuAction GetShaderMenu(Window wnd, Func<string> getter, Action<string> setter, bool allowPreview)
	{
		List<string> shaderFiles = Directory.GetFiles(ConfigManager.ShaderFolder, "*.slangp", SearchOption.AllDirectories).ToList();
		shaderFiles.Sort(StringComparer.OrdinalIgnoreCase);

		return new MainMenuAction() {
			ActionType = ActionType.Shader,
			IsVisible = () => ConfigApi.CheckShaderSupport(),
			SubActions = new() {
				new MainMenuAction() {
					ActionType = ActionType.ClearShader,
					HintText = () => Path.GetFileNameWithoutExtension(getter()),
					IsEnabled = () => !string.IsNullOrWhiteSpace(getter()),
					OnClick = () => {
						LoadShader("", getter, setter);
					}
				},
				new MainMenuAction() {
					ActionType = ActionType.ShaderSettings,
					HintText = () => Path.GetFileNameWithoutExtension(getter()),
					IsEnabled = () => File.Exists(getter()) && ConfigApi.GetShaderParams(getter()).Length > 0,
					OnClick = () => {
						InteropShaderParam[] shaderParams = ConfigApi.GetShaderParams(getter());
						new ShaderConfigWindow() {
							DataContext = new ShaderConfigViewModel(allowPreview, getter())
						}.ShowCenteredDialog((Control)wnd);
					}
				},
				new ContextMenuSeparator(),
				new MainMenuAction() {
					ActionType = ActionType.LoadShader,
					OnClick = async () => {
						string? shader = await FileDialogHelper.OpenFile(ConfigManager.ShaderFolder, wnd, FileDialogHelper.ShaderExt);
						if(shader != null) {
							LoadShader(shader, getter, setter);
						}
					}
				},
				new MainMenuAction() {
					ActionType = ActionType.RecentShaders,
					IsEnabled = () => ConfigManager.Config.RecentFiles.Shaders.Count > 0,
					SubActions = new List<object>() {
						GetRecentShaderItem(0, getter, setter),
						GetRecentShaderItem(1, getter, setter),
						GetRecentShaderItem(2, getter, setter),
						GetRecentShaderItem(3, getter, setter),
						GetRecentShaderItem(4, getter, setter),
						GetRecentShaderItem(5, getter, setter),
						GetRecentShaderItem(6, getter, setter),
						GetRecentShaderItem(7, getter, setter),
						GetRecentShaderItem(8, getter, setter),
						GetRecentShaderItem(9, getter, setter)
					}
				},
				new MainMenuAction() {
					ActionType = ActionType.AllShaders,
					IsEnabled = () => true,
					SubActions = GenerateShaderSubMenu(BuildShaderTree(shaderFiles), getter, setter)
				},
				new ContextMenuSeparator(),
				new MainMenuAction() {
					ActionType = ActionType.OpenShaderFolder,
					OnClick = () => OpenShaderFolder()
				}
			}
		};
	}

	private static FolderNode BuildShaderTree(List<string> shaderFiles)
	{
		FolderNode root = new FolderNode();

		char[] separator = ['/', '\\'];

		foreach(string shaderFile in shaderFiles) {
			string relPath = Path.GetRelativePath(ConfigManager.ShaderFolder, shaderFile);
			string[] parts = relPath.Split(separator, StringSplitOptions.RemoveEmptyEntries);
			if(parts.Length == 0) {
				continue;
			}

			List<FolderNode> currentList = root.Children;
			FolderNode? currentFolder = root;

			for(int i = 0; i < parts.Length - 1; i++) {
				string folderName = parts[i];
				FolderNode? existingNode = null;

				if(currentList.Count > 0 && currentList[^1].Name.Equals(folderName, StringComparison.OrdinalIgnoreCase)) {
					existingNode = currentList[^1];
				} else {
					existingNode = new FolderNode { Name = folderName };
					currentList.Add(existingNode);
				}

				currentFolder = existingNode;
				currentList = existingNode.Children;
			}

			if(currentFolder != null) {
				string fileName = parts[^1];
				currentFolder.Files.Add(shaderFile);
			}
		}

		return root;
	}

	private static List<object> GenerateShaderSubMenu(FolderNode node, Func<string> getter, Action<string> setter)
	{
		List<object> subActions = new();
		if(node.Children.Count == 0 && node.Files.Count == 0) {
			subActions.Add(new MainMenuAction() {
				ActionType = ActionType.NoShadersFound,
				OnClick = () => OpenShaderFolder()
			});
			return subActions;
		}

		foreach(FolderNode child in node.Children) {
			subActions.Add(new MainMenuAction() {
				ActionType = ActionType.ShaderFolder,
				DynamicText = () => child.Name,
				SubActions = GenerateShaderSubMenu(child, getter, setter)
			});
		}

		foreach(string file in node.Files) {
			subActions.Add(new MainMenuAction() {
				CustomText = Path.GetFileNameWithoutExtension(file),
				IsSelected = () => getter() == file,
				OnClick = () => {
					LoadShader(file, getter, setter);
				}
			});
		}

		return subActions;
	}

	private static MainMenuAction GetRecentShaderItem(int index, Func<string> getter, Action<string> setter)
	{
		Func<int, string?> getRecentFile = (int index) => {
			return ConfigManager.Config.RecentFiles.Shaders.ElementAtOrDefault(index);
		};

		return new MainMenuAction() {
			ActionType = ActionType.Custom,
			DynamicText = () => Path.GetFileNameWithoutExtension(getRecentFile(index)) ?? "",
			IsVisible = () => getRecentFile(index) != null,
			IsSelected = () => getRecentFile(index) == getter(),
			OnClick = () => {
				LoadShader(getRecentFile(index) ?? "", getter, setter);
			}
		};
	}

	private static void LoadShader(string shaderFile, Func<string> getter, Action<string> setter)
	{
		setter(shaderFile);
		if(!string.IsNullOrWhiteSpace(shaderFile)) {
			ConfigManager.Config.RecentFiles.AddRecentShader(shaderFile);
		}
		ConfigManager.Config.Video.ApplyConfig();
	}

	private static void OpenShaderFolder()
	{
		System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo() {
			FileName = ConfigManager.ShaderFolder + Path.DirectorySeparatorChar,
			UseShellExecute = true,
			Verb = "open"
		});
	}

	private class FolderNode
	{
		public string Name { get; set; } = "";
		public List<FolderNode> Children { get; set; } = new();
		public List<string> Files { get; set; } = new();
	}
}
