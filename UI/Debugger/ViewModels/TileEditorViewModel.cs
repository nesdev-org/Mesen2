using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using Mesen.Config;
using Mesen.Debugger.Controls;
using Mesen.Debugger.Utilities;
using Mesen.Debugger.Windows;
using Mesen.Interop;
using Mesen.Utilities;
using Mesen.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;

namespace Mesen.Debugger.ViewModels;

public partial class TileEditorViewModel : DisposableViewModel
{
	[ObservableProperty] public partial DynamicBitmap ViewerBitmap { get; private set; }

	[ObservableProperty] public partial UInt32[] PaletteColors { get; set; } = Array.Empty<UInt32>();
	[ObservableProperty] public partial UInt32[] RawPalette { get; set; } = Array.Empty<UInt32>();
	[ObservableProperty] public partial RawPaletteFormat RawFormat { get; set; }
	[ObservableProperty] public partial int PaletteColumnCount { get; private set; } = 16;
	[ObservableProperty] public partial int SelectedColor { get; set; } = 0;
	[ObservableProperty] public partial List<GridDefinition>? CustomGrids { get; set; } = null;

	public TileEditorConfig Config { get; }

	public List<ContextMenuAction> FileMenuActions { get; private set; } = new();
	public List<ContextMenuAction> ViewMenuActions { get; private set; } = new();
	public List<ContextMenuAction> ToolsMenuActions { get; private set; } = new();
	public List<ContextMenuAction> ToolbarActions { get; private set; } = new();

	private CpuType _cpuType;
	private List<TileAddressInfo> _tileAddresses;
	private int _columnCount = 1;
	private int _rowCount = 1;
	private TileFormat _tileFormat;
	private UInt32[] _tileBuffer = Array.Empty<UInt32>();

	[Obsolete("For designer only")]
	public TileEditorViewModel() : this(new() { new() }, 1, TileFormat.Bpp4, 0) { }

	public TileEditorViewModel(List<TileAddressInfo> tileAddresses, int columnCount, TileFormat format, int initialPalette)
	{
		Config = ConfigManager.Config.Debug.TileEditor;
		_tileAddresses = tileAddresses;
		_columnCount = columnCount;
		_rowCount = tileAddresses.Count / columnCount;
		_cpuType = tileAddresses[0].Address.Type.ToCpuType();
		_tileFormat = format;
		SelectedColor = initialPalette * GetColorsPerPalette(_tileFormat);

		PixelSize size = format.GetTileSize();
		_tileBuffer = new UInt32[size.Width * size.Height];
		ViewerBitmap = new DynamicBitmap(new PixelSize(size.Width * _columnCount, size.Height * _rowCount), new Vector(96, 96), PixelFormat.Bgra8888, AlphaFormat.Premul);

		if(Design.IsDesignMode) {
			return;
		}

		AddDisposable(Config.ObserveProp(nameof(Config.Background), RefreshViewer));
		AddDisposable(this.ObserveProp(nameof(SelectedColor), RefreshViewer));
		AddDisposable(Config.ObserveProp(nameof(Config.ShowGrid), () => {
			if(Config.ShowGrid) {
				PixelSize tileSize = _tileFormat.GetTileSize();
				CustomGrids = new List<GridDefinition>() { new GridDefinition() {
					SizeX = tileSize.Width,
					SizeY = tileSize.Height,
					Color = Color.FromArgb(192, Colors.LightBlue.R, Colors.LightBlue.G, Colors.LightBlue.B)
				} };
			} else {
				CustomGrids = null;
			}
		}));
		AddDisposable(Config.ObserveProp(nameof(Config.ImageScale), () => {
			if(Config.ImageScale < 4) {
				Config.ImageScale = 4;
			}
		}));

		RefreshViewer();
	}

	public void InitActions(PictureViewer picViewer, Window wnd)
	{
		FileMenuActions = AddDisposables(new List<ContextMenuAction>() {
			new ContextMenuAction() {
				ActionType = ActionType.ExportToPng,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.SaveAsPng),
				OnClick = () => picViewer.ExportToPng()
			},
			new ContextMenuAction() {
				ActionType = ActionType.CopyToClipboard,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.Copy),
				OnClick = () => picViewer.CopyToClipboard()
			},
			new ContextMenuSeparator(),
			new ContextMenuAction() {
				ActionType = ActionType.Exit,
				OnClick = () => wnd.Close()
			}
		});

		ViewMenuActions = AddDisposables(new List<ContextMenuAction>() {
			new ContextMenuAction() {
				ActionType = ActionType.ZoomIn,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.ZoomIn),
				OnClick = () => picViewer.ZoomIn()
			},
			new ContextMenuAction() {
				ActionType = ActionType.ZoomOut,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.ZoomOut),
				OnClick = () => picViewer.ZoomOut()
			},
		});

		ToolsMenuActions = AddDisposables(GetTools());
		ToolbarActions = AddDisposables(GetTools());

		DebugShortcutManager.RegisterActions(wnd, FileMenuActions);
		DebugShortcutManager.RegisterActions(wnd, ViewMenuActions);
		DebugShortcutManager.RegisterActions(wnd, ToolsMenuActions);
	}

	private List<ContextMenuAction> GetTools()
	{
		return new List<ContextMenuAction>() {
			new ContextMenuAction() {
				ActionType = ActionType.FlipHorizontal,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.TileEditor_FlipHorizontal),
				OnClick = () => Transform(TransformType.FlipHorizontal)
			},
			new ContextMenuAction() {
				ActionType = ActionType.FlipVertical,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.TileEditor_FlipVertical),
				OnClick = () => Transform(TransformType.FlipVertical)
			},

			new ContextMenuSeparator(),

			new ContextMenuAction() {
				ActionType = ActionType.RotateLeft,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.TileEditor_RotateLeft),
				IsEnabled = () => _columnCount == _rowCount,
				OnClick = () => Transform(TransformType.RotateLeft)
			},
			new ContextMenuAction() {
				ActionType = ActionType.RotateRight,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.TileEditor_RotateRight),
				IsEnabled = () => _columnCount == _rowCount,
				OnClick = () => Transform(TransformType.RotateRight)
			},

			new ContextMenuSeparator(),

			new ContextMenuAction() {
				ActionType = ActionType.TranslateLeft,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.TileEditor_TranslateLeft),
				OnClick = () => Transform(TransformType.TranslateLeft)
			},
			new ContextMenuAction() {
				ActionType = ActionType.TranslateRight,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.TileEditor_TranslateRight),
				OnClick = () => Transform(TransformType.TranslateRight)
			},
			new ContextMenuAction() {
				ActionType = ActionType.TranslateUp,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.TileEditor_TranslateUp),
				OnClick = () => Transform(TransformType.TranslateUp)
			},
			new ContextMenuAction() {
				ActionType = ActionType.TranslateDown,
				Shortcut = () => ConfigManager.Config.Debug.Shortcuts.Get(DebuggerShortcut.TileEditor_TranslateDown),
				OnClick = () => Transform(TransformType.TranslateDown)
			},
		};
	}

	private List<int> GetTileData()
	{
		PixelSize tileSize = _tileFormat.GetTileSize();
		int width = tileSize.Width * _columnCount;
		int height = tileSize.Height * _rowCount;

		List<int> data = new List<int>(width * height);
		for(int y = 0; y < height; y++) {
			for(int x = 0; x < width; x++) {
				data.Add(GetColorAtPosition(new PixelPoint(x, y)));
			}
		}

		return data;
	}

	private void Transform(TransformType type)
	{
		List<int> data = GetTileData();

		PixelSize tileSize = _tileFormat.GetTileSize();
		int width = tileSize.Width * _columnCount;
		int height = tileSize.Height * _rowCount;
		for(int y = 0; y < height; y++) {
			for(int x = 0; x < width; x++) {
				int newY = type switch {
					TransformType.FlipVertical => height - y - 1,
					TransformType.FlipHorizontal => y,
					TransformType.RotateLeft => x,
					TransformType.RotateRight => height - x - 1,
					TransformType.TranslateLeft => y,
					TransformType.TranslateRight => y,
					TransformType.TranslateUp => y < height - 1 ? (y + 1) : 0,
					TransformType.TranslateDown => y > 0 ? (y - 1) : height - 1,
					_ => y
				};

				int newX = type switch {
					TransformType.FlipVertical => x,
					TransformType.FlipHorizontal => width - x - 1,
					TransformType.RotateLeft => width - y - 1,
					TransformType.RotateRight => y,
					TransformType.TranslateLeft => x < width - 1 ? (x + 1) : 0,
					TransformType.TranslateRight => x > 0 ? (x - 1) : width - 1,
					TransformType.TranslateUp => x,
					TransformType.TranslateDown => x,
					_ => x
				};

				SetPixelColor(new PixelPoint(x, y), data[newY * width + newX]);
			}
		}

		RefreshViewer();
	}

	private record TilePixelPositionInfo(int Column, int Row, int TileX, int TileY);
	private TilePixelPositionInfo GetPositionInfo(PixelPoint position)
	{
		PixelSize tileSize = _tileFormat.GetTileSize();

		int column = position.X / tileSize.Width;
		int row = position.Y / tileSize.Height;
		int tileX = position.X % tileSize.Width;
		int tileY = position.Y % tileSize.Height;

		TileAddressInfo tile = _tileAddresses[column + row * _columnCount];
		if(tile.HorizontalMirroring) {
			tileX = (tileSize.Width - tileX) - 1;
		}
		if(tile.VerticalMirroring) {
			tileY = (tileSize.Height - tileY) - 1;
		}

		return new(column, row, tileX, tileY);
	}

	public int GetColorAtPosition(PixelPoint position)
	{
		TilePixelPositionInfo pos = GetPositionInfo(position);
		int paletteColorIndex = DebugApi.GetTilePixel(_tileAddresses[pos.Column + pos.Row * _columnCount].Address, _tileFormat, pos.TileX, pos.TileY);
		return SelectedColor - (SelectedColor % GetColorsPerPalette(_tileFormat)) + paletteColorIndex;
	}

	public void SelectColor(PixelPoint position)
	{
		SelectedColor = GetColorAtPosition(position);
	}

	private void SetPixelColor(PixelPoint position, int color)
	{
		TilePixelPositionInfo pos = GetPositionInfo(position);
		DebugApi.SetTilePixel(_tileAddresses[pos.Column + pos.Row * _columnCount].Address, _tileFormat, pos.TileX, pos.TileY, color);
	}

	public void UpdatePixel(PixelPoint position, bool clearPixel)
	{
		int pixelColor = clearPixel ? 0 : SelectedColor % GetColorsPerPalette(_tileFormat);
		SetPixelColor(position, pixelColor);
		RefreshViewer();
	}

	private unsafe void RefreshViewer()
	{
		Dispatcher.UIThread.Post((Action)(() => {
			DebugPaletteInfo palette = DebugApi.GetPaletteInfo(_cpuType, new GetPaletteInfoOptions() { Format = _tileFormat });
			PaletteColors = palette.GetRgbPalette();
			RawPalette = palette.GetRawPalette();
			RawFormat = palette.RawFormat;
			PaletteColumnCount = PaletteColors.Length > 16 ? 16 : 4;

			PixelSize tileSize = _tileFormat.GetTileSize();
			int bytesPerTile = _tileFormat.GetBytesPerTile();

			using(var framebuffer = ViewerBitmap.Lock()) {
				for(int y = 0; y < _rowCount; y++) {
					for(int x = 0; x < _columnCount; x++) {
						fixed(UInt32* ptr = _tileBuffer) {
							TileAddressInfo addr = _tileAddresses[y * _columnCount + x];
							byte[] sourceData = DebugApi.GetMemoryValues(addr.Address.Type, (uint)addr.Address.Address, (uint)(addr.Address.Address + bytesPerTile - 1));
							DebugApi.GetTileView(_cpuType, GetOptions(x, y), sourceData, sourceData.Length, PaletteColors, (IntPtr)ptr);
							UInt32* viewer = (UInt32*)framebuffer.FrameBuffer.Address;
							int rowPitch = ViewerBitmap.PixelSize.Width;
							int baseOffset = x * tileSize.Width + y * tileSize.Height * rowPitch;
							for(int j = 0; j < tileSize.Height; j++) {
								int yDest = addr.VerticalMirroring ? (tileSize.Height - j - 1) : j;
								for(int i = 0; i < tileSize.Width; i++) {
									int xDest = addr.HorizontalMirroring ? (tileSize.Width - i - 1) : i;
									viewer[baseOffset + yDest * rowPitch + xDest] = ptr[j * tileSize.Width + i];
								}
							}
						}
					}
				}
			}
		}));
	}

	public int GetColorsPerPalette()
	{
		return GetColorsPerPalette(_tileFormat);
	}

	private int GetColorsPerPalette(TileFormat format)
	{
		return format.GetBitsPerPixel() switch {
			1 => 2, //2-color palettes
			2 => 4, //4-color palettes
			4 => 16, //16-color palettes
			_ => 256
		};
	}

	private GetTileViewOptions GetOptions(int column, int row)
	{
		int tileIndex = row * _columnCount + column;

		return new GetTileViewOptions() {
			MemType = _tileAddresses[tileIndex].Address.Type,
			Format = _tileFormat,
			Width = _tileFormat.GetTileSize().Width / 8,
			Height = _tileFormat.GetTileSize().Height / 8,
			Palette = SelectedColor / GetColorsPerPalette(_tileFormat),
			Layout = TileLayout.Normal,
			Filter = TileFilter.None,
			StartAddress = _tileAddresses[tileIndex].Address.Address,
			Background = Config.Background,
			UseGrayscalePalette = false
		};
	}

	public enum TransformType
	{
		FlipHorizontal,
		FlipVertical,
		RotateLeft,
		RotateRight,
		TranslateLeft,
		TranslateRight,
		TranslateUp,
		TranslateDown,
	}
}

public class TileAddressInfo
{
	public AddressInfo Address { get; set; }
	public bool HorizontalMirroring { get; set; }
	public bool VerticalMirroring { get; set; }
}
