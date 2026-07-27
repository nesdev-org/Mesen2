using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Interop;

namespace Mesen.Config
{
	public partial class TilemapViewerConfig : BaseWindowConfig<TilemapViewerConfig>
	{
		[ObservableProperty] public partial bool ShowSettingsPanel { get; set; } = true;

		[ObservableProperty] public partial double ImageScale { get; set; } = 1;

		[ObservableProperty] public partial bool ShowGrid { get; set; }
		[ObservableProperty] public partial bool ShowScrollOverlay { get; set; }

		[ObservableProperty] public partial bool NesShowAttributeGrid { get; set; }
		[ObservableProperty] public partial bool NesShowAttributeByteGrid { get; set; }
		[ObservableProperty] public partial bool NesShowTilemapGrid { get; set; }

		[ObservableProperty] public partial TilemapHighlightMode TileHighlightMode { get; set; } = TilemapHighlightMode.None;
		[ObservableProperty] public partial TilemapHighlightMode AttributeHighlightMode { get; set; } = TilemapHighlightMode.None;
		[ObservableProperty] public partial TilemapDisplayMode DisplayMode { get; set; } = TilemapDisplayMode.Default;
		[ObservableProperty] public partial TilemapBackground Background { get; set; } = TilemapBackground.Default;

		[ObservableProperty] public partial RefreshTimingConfig RefreshTiming { get; set; } = new();

		public TilemapViewerConfig()
		{
		}
	}
}
