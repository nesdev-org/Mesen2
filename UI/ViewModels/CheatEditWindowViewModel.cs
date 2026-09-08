using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Config;
using Mesen.Interop;
using Mesen.Utilities;
using Mesen.Windows;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Mesen.ViewModels
{
	public partial class CheatEditWindowViewModel : DisposableViewModel
	{
		public CheatCode Cheat { get; }

		[ObservableProperty] public partial string ConvertedCodes { get; private set; } = "";
		[ObservableProperty] public partial bool ShowInvalidCodeHint { get; private set; } = false;
		[ObservableProperty] public partial bool OkButtonEnabled { get; private set; } = false;

		[ObservableProperty] public partial Enum[] AvailableCheatTypes { get; private set; } = Array.Empty<Enum>();

		[Obsolete("For designer only")]
		public CheatEditWindowViewModel() : this(new CheatCode(), () => { }) { }

		public CheatEditWindowViewModel(CheatCode cheat, Action closeWindow)
		{
			Cheat = cheat;

			if(Design.IsDesignMode) {
				return;
			}

			AvailableCheatTypes = Enum.GetValues<CheatType>().Where(e => MainWindowViewModel.Instance.RomInfo.CpuTypes.Contains(e.ToCpuType())).Cast<Enum>().ToArray();
			if(!AvailableCheatTypes.Contains(Cheat.Type)) {
				Cheat.Type = (CheatType)AvailableCheatTypes[0];
			}

			//Close popup window if the loaded game changes
			string romPath = MainWindowViewModel.Instance.RomInfo.RomPath;
			AddDisposable(MainWindowViewModel.Instance.ObserveProp(nameof(RomInfo), () => {
				if(romPath != MainWindowViewModel.Instance.RomInfo.RomPath) {
					closeWindow();
				}
			}));

			AddDisposable(Cheat.ObserveProp([nameof(CheatCode.Codes), nameof(CheatCode.Type)], () => {
				string[] codes = cheat.Codes.Split(Environment.NewLine);
				StringBuilder sb = new StringBuilder();
				bool hasInvalidCode = false;
				bool hasValidCode = false;
				foreach(string codeString in codes) {
					if(sb.Length > 0) {
						sb.Append(Environment.NewLine);
					}

					InteropInternalCheatCode code = new();
					if(EmuApi.GetConvertedCheat(new InteropCheatCode(cheat.Type, codeString.Trim()), ref code)) {
						if(code.IsAbsoluteAddress) {
							sb.Append(code.MemType.GetShortName() + " ");
						}

						if(code.Compare >= 0) {
							sb.Append($"{code.Address:X2}:{code.Value:X2}:{code.Compare:X2}");
						} else {
							sb.Append($"{code.Address:X2}:{code.Value:X2}");
						}
						hasValidCode = true;
					} else {
						if(!string.IsNullOrWhiteSpace(codeString)) {
							hasInvalidCode = true;
							sb.Append("[invalid code]");
						}
					}
				}

				ShowInvalidCodeHint = hasInvalidCode;
				OkButtonEnabled = hasValidCode && !hasInvalidCode;

				ConvertedCodes = sb.ToString();
			}));
		}
	}
}
