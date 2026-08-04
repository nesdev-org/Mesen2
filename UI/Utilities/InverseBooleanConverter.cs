using Avalonia.Data.Converters;
using System;
using System.Globalization;

namespace Mesen.Utilities
{
	public class InverseBooleanConverter : IValueConverter
	{
		public object? Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
		{
			return value is bool b ? !b : value;
		}

		public object? ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
		{
			return value is bool b ? !b : value;
		}
	}
}
