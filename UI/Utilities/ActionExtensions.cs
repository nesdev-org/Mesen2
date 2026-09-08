using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Mesen.Utilities;

public static class ActionExtensions
{
	public static Action Debounce(this Action action, int milliseconds = 300)
	{
		CancellationTokenSource? cancelSrc = null;

		return () => {
			cancelSrc?.Cancel();
			cancelSrc = new CancellationTokenSource();

			Task t = Task.Delay(milliseconds, cancelSrc.Token);
			t.ContinueWith(
				task => {
					if(task.IsCompletedSuccessfully) {
						action();
					}
				}
			);
		};
	}
}
