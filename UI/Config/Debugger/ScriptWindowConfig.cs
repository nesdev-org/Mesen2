using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Mesen.Config
{
	public partial class ScriptWindowConfig : BaseWindowConfig<ScriptWindowConfig>
	{
		private const int MaxRecentScripts = 10;

		[ObservableProperty] public partial List<string> RecentScripts { get; set; } = new List<string>();

		[ObservableProperty] public partial int Zoom { get; set; } = 100;

		[ObservableProperty] public partial double LogWindowHeight { get; set; } = 100;

		[ObservableProperty] public partial ScriptStartupBehavior ScriptStartupBehavior { get; set; } = ScriptStartupBehavior.ShowTutorial;
		[ObservableProperty] public partial bool SaveScriptBeforeRun { get; set; } = true;
		[ObservableProperty] public partial bool AutoStartScriptOnLoad { get; set; } = true;
		[ObservableProperty] public partial bool AutoReloadScriptWhenFileChanges { get; set; } = true;
		[ObservableProperty] public partial bool AutoRestartScriptAfterPowerCycle { get; set; } = true;

		[ObservableProperty] public partial bool AllowIoOsAccess { get; set; } = false;
		[ObservableProperty] public partial bool AllowNetworkAccess { get; set; } = false;

		[ObservableProperty] public partial bool ShowLineNumbers { get; set; } = true;

		[ObservableProperty] public partial UInt32 ScriptTimeout { get; set; } = 1;

		public void AddRecentScript(string scriptFile)
		{
			string? existingItem = RecentScripts.Where((file) => file == scriptFile).FirstOrDefault();
			if(existingItem != null) {
				RecentScripts.Remove(existingItem);
			}

			RecentScripts.Insert(0, scriptFile);
			if(RecentScripts.Count > ScriptWindowConfig.MaxRecentScripts) {
				RecentScripts.RemoveAt(ScriptWindowConfig.MaxRecentScripts);
			}
		}
	}

	public enum ScriptStartupBehavior
	{
		ShowTutorial = 0,
		ShowBlankWindow = 1,
		LoadLastScript = 2
	}
}
