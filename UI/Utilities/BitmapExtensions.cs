using Avalonia;
using Avalonia.Media.Imaging;
using Avalonia.Platform;

namespace Mesen.Utilities;

static class BitmapExtensions
{
	public static Bitmap CropBitmap(this Bitmap src, PixelRect rect)
	{
		PixelSize size = new PixelSize(rect.Width, rect.Height);
		WriteableBitmap bmp = new WriteableBitmap(size, src.Dpi, src.Format, src.AlphaFormat);
		using(ILockedFramebuffer dst = bmp.Lock()) {
			src.CopyPixels(rect, dst.Address, dst.RowBytes * dst.Size.Height, dst.RowBytes);
		}
		return bmp;
	}

}
