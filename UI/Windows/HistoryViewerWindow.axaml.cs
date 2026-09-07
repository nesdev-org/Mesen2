using Avalonia;
using Avalonia.Controls;
using Avalonia.Data;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using Mesen.Config;
using Mesen.Controls;
using Mesen.Interop;
using Mesen.Utilities;
using Mesen.ViewModels;
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace Mesen.Windows
{
	public class HistoryViewerWindow : MesenWindow
	{
		private HistoryViewerViewModel _model;
		private DispatcherTimer _timer;
		private DispatcherTimer _mouseTimer;

		private NativeRenderer _renderer;
		private SoftwareRendererView _softwareRenderer;
		private Panel _rendererPanel;
		private Size _rendererSize;

		private Border _controlBar;
		private Menu _mainMenu;
		private bool _prevLeftPressed;
		private NotificationListener? _listener;

		static HistoryViewerWindow()
		{
			WindowStateProperty.Changed.AddClassHandler<HistoryViewerWindow>((x, e) => x.OnWindowStateChanged());
		}

		public HistoryViewerWindow()
		{
			_model = new HistoryViewerViewModel();
			DataContext = _model;

			InitializeComponent();

			_renderer = this.GetControl<NativeRenderer>("Renderer");
			_softwareRenderer = this.GetControl<SoftwareRendererView>("SoftwareRenderer");
			_rendererPanel = this.GetControl<Panel>("RendererPanel");
			_rendererPanel.LayoutUpdated += RendererPanel_LayoutUpdated;

			_controlBar = this.GetControl<Border>("ControlBar");
			_mainMenu = this.GetControl<Menu>("ActionMenu");
			_timer = new DispatcherTimer(TimeSpan.FromMilliseconds(50), DispatcherPriority.Normal, (s, e) => {
				_model.Update();
			});

			_mouseTimer = new DispatcherTimer(TimeSpan.FromMilliseconds(15), DispatcherPriority.Normal, (s, e) => {
				UpdateMouse();
			});

			ConfigManager.Config.HistoryViewer.LoadWindowSettings(this);
		}

		private void UpdateMouse()
		{
			if(MenuHelper.IsPointerInMenu(_mainMenu)) {
				return;
			}

			bool usesSoftwareRenderer = _model.IsSoftwareRendererVisible;

			SystemMouseState mouseState = InputApi.GetSystemMouseState(usesSoftwareRenderer ? IntPtr.Zero : _renderer.Handle);
			PixelPoint mousePos = new PixelPoint(mouseState.XPosition, mouseState.YPosition);

			PixelPoint rendererTopLeft;
			PixelRect rendererScreenRect;
			if(usesSoftwareRenderer) {
				rendererTopLeft = _softwareRenderer.PointToScreen(new Point());
				rendererScreenRect = new PixelRect(rendererTopLeft, PixelSize.FromSize(_softwareRenderer.Bounds.Size, LayoutHelper.GetLayoutScale(this) / InputApi.GetPixelScale()));
			} else {
				rendererTopLeft = _renderer.PointToScreen(new Point());
				rendererScreenRect = new PixelRect(rendererTopLeft, PixelSize.FromSize(_renderer.Bounds.Size, LayoutHelper.GetLayoutScale(this) / InputApi.GetPixelScale()));
			}

			if(rendererScreenRect.Contains(mousePos)) {
				if(mouseState.LeftButton && !_prevLeftPressed) {
					if(_mainMenu.IsOpen) {
						_mainMenu.Close();
					} else {
						_model.TogglePause();
					}
				}
			}

			_prevLeftPressed = mouseState.LeftButton;
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		protected override void OnOpened(EventArgs e)
		{
			base.OnOpened(e);

			if(Design.IsDesignMode) {
				return;
			}

			HistoryApi.HistoryViewerInitialize(TryGetPlatformHandle()?.Handle ?? IntPtr.Zero, _renderer.Handle);
			_model.SetCoreOptions();
			_model.Update();
			_model.InitActions(this);
			_timer.Start();
			_mouseTimer.Start();

			_listener = new NotificationListener(forHistoryViewer: true);
			_listener.OnNotification += OnNotification;
		}

		protected override void OnClosing(WindowClosingEventArgs e)
		{
			base.OnClosing(e);
			if(Design.IsDesignMode) {
				return;
			}
			_timer.Stop();
			_mouseTimer.Stop();
			_listener?.Dispose();
			HistoryApi.HistoryViewerRelease();
			ConfigManager.Config.HistoryViewer.SaveWindowSettings(this);
		}

		public void SetScale(double scale)
		{
			//TODOv2 - Calling this twice seems to fix what might be an issue in Avalonia?
			//On the first call, when DPI > 100%, sometimes _rendererPanel's bounds are incorrect
			InternalSetScale(scale);
			InternalSetScale(scale);
		}

		private void InternalSetScale(double scale)
		{
			double dpiScale = LayoutHelper.GetLayoutScale(this);
			double aspectRatio = EmuApi.GetAspectRatio();

			FrameInfo screenSize = EmuApi.GetBaseScreenSize();
			if(WindowState == WindowState.Normal) {
				_rendererSize = new Size();

				double menuHeight = _mainMenu.Bounds.Height;

				double width = Math.Max(MinWidth, Math.Round(screenSize.Height * aspectRatio * scale) / dpiScale);
				double height = Math.Max(MinHeight, screenSize.Height * scale / dpiScale);
				Width = width;
				Height = height + menuHeight + _controlBar.Bounds.Height;
				ResizeRenderer();
			} else if(WindowState == WindowState.Maximized || WindowState == WindowState.FullScreen) {
				_rendererSize = new Size(Math.Floor(screenSize.Width * scale * aspectRatio) / dpiScale, Math.Floor(screenSize.Height * scale) / dpiScale);
				ResizeRenderer();
			}
		}

		private void ResizeRenderer()
		{
			_rendererPanel.InvalidateMeasure();
			_rendererPanel.InvalidateArrange();
		}

		private void RendererPanel_LayoutUpdated(object? sender, EventArgs e)
		{
			double aspectRatio = EmuApi.GetAspectRatio();
			double dpiScale = LayoutHelper.GetLayoutScale(this);

			Size finalSize = _rendererSize == default ? _rendererPanel.Bounds.Size : _rendererSize;
			double height = finalSize.Height;
			double width = finalSize.Height * aspectRatio;
			if(Math.Round(width) > Math.Round(finalSize.Width)) {
				//Use renderer width to calculate the height instead of the opposite
				//when current window dimensions would cause cropping horizontally
				//if the screen width was calculated based on the height.
				width = finalSize.Width;
				height = width / aspectRatio;
			}

			if(ConfigManager.Config.Video.FullscreenForceIntegerScale && this.GetWindow() is Window wnd && (wnd.WindowState == WindowState.FullScreen || wnd.WindowState == WindowState.Maximized)) {
				FrameInfo baseSize = EmuApi.GetBaseScreenSize();
				double scale = height * dpiScale / baseSize.Height;
				if(scale != Math.Floor(scale)) {
					height = baseSize.Height * Math.Max(1, Math.Floor(scale / dpiScale));
					width = height * aspectRatio;
				}
			}

			uint realWidth = (uint)Math.Round(width * dpiScale);
			uint realHeight = (uint)Math.Round(height * dpiScale);

			//Make sure the height & width are always a multiple of 2 pixels
			//This prevents issues with shaders, where a vertical or horizontal
			//line artifact can appear in the middle of the frame if odd.
			if((realWidth & 0x01) != 0) {
				realWidth++;
			}
			if((realHeight & 0x01) != 0) {
				realHeight++;
			}

			width = Math.Round(realWidth / dpiScale, 8, MidpointRounding.ToZero) + 0.00001;
			height = Math.Round(realHeight / dpiScale, 8, MidpointRounding.ToZero) + 0.00001;

			_model.RendererSize = new Size(
				realWidth,
				realHeight
			);

			_renderer.Width = width;
			_renderer.Height = height;

			//Position the renderer in the center
			Canvas.SetTop(_renderer, finalSize.Height > _renderer.Height ? (finalSize.Height - _renderer.Height) / 2 : 0);
			Canvas.SetLeft(_renderer, finalSize.Width > _renderer.Width ? (finalSize.Width - _renderer.Width) / 2 : 0);

			_model.SoftwareRenderer.Width = width;
			_model.SoftwareRenderer.Height = height;
		}

		private void OnWindowStateChanged()
		{
			_rendererSize = new Size();
			ResizeRenderer();
		}

		private void OnNotification(NotificationEventArgs e)
		{
			switch(e.NotificationType) {
				case ConsoleNotificationType.RefreshSoftwareRenderer:
					SoftwareRendererFrame frame = Marshal.PtrToStructure<SoftwareRendererFrame>(e.Parameter);
					_softwareRenderer.UpdateSoftwareRenderer(frame);
					break;
			}
		}
	}
}
