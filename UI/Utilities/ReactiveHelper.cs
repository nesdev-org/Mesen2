using Avalonia;
using Avalonia.Reactive;
using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Reflection;

namespace Mesen.Utilities
{
	public static class ReactiveHelper
	{
		public static IDisposable RegisterObserver<T>(T target, string propertyName, Action action) where T : INotifyPropertyChanged
		{
			return RegisterObserver([target], [propertyName], action);
		}

		public static IDisposable RegisterObserver<T>(T[] targets, string propertyName, Action action) where T : INotifyPropertyChanged
		{
			return RegisterObserver(targets, [propertyName], action);
		}

		public static IDisposable RegisterObserver<T>(T target, string[] properties, Action action) where T : INotifyPropertyChanged
		{
			return RegisterObserver([target], properties, action);
		}

		public static IDisposable RegisterObserver<T>(T[] targets, string[] properties, Action action) where T : INotifyPropertyChanged
		{
			HashSet<string> props = new(properties);
			PropertyChangedEventHandler handler = (s, e) => {
				if(e.PropertyName != null && props.Contains(e.PropertyName)) {
					action();
				}
			};
			foreach(T target in targets) {
				target.PropertyChanged += handler;
			}
			action();

			return new DisposableObserver(() => {
				foreach(INotifyPropertyChanged target in targets) {
					target.PropertyChanged -= handler;
				}
			});
		}

		public static IDisposable RegisterForeignObserver((Func<ObservableObject>, string)[] targets, Action action)
		{
			List<Action> elementsToDispose = new();
			Action dispose = () => {
				foreach(Action disposeAction in elementsToDispose) {
					disposeAction();
				}
			};

			Action? initObserver = null;

			initObserver = () => {
				foreach((Func<ObservableObject> targetFunc, string prop) in targets) {
					PropertyChangedEventHandler handler = (s, e) => {
						if(e.PropertyName == prop) {
							action();
							dispose();
							initObserver?.Invoke();
						}
					};
					ObservableObject target = targetFunc();
					target.PropertyChanged += handler;
					elementsToDispose.Add(() => { target.PropertyChanged -= handler; });
				}
			};

			initObserver();
			action();

			return new DisposableObserver(dispose);
		}

		private static PropertyInfo[] GetProperties(ObservableObject target)
		{
#pragma warning disable IL2075 // 'this' argument does not satisfy 'DynamicallyAccessedMembersAttribute' in call to target method. The return value of the source method does not have matching annotations.
			return target.GetType().GetProperties(BindingFlags.Instance | BindingFlags.Public);
#pragma warning restore IL2075 // 'this' argument does not satisfy 'DynamicallyAccessedMembersAttribute' in call to target method. The return value of the source method does not have matching annotations.
		}

		public static IDisposable RegisterRecursiveObserver(ObservableObject target, PropertyChangedEventHandler handler)
		{
			Dictionary<string, ObservableObject> observableObjects = new();
			Dictionary<ObservableObject, IDisposable> disposables = new();
			Dictionary<string, PropertyInfo> props = new();

			foreach(PropertyInfo prop in GetProperties(target)) {
				if(prop.GetCustomAttribute<ObservablePropertyAttribute>() != null) {
					object? value = prop.GetValue(target);
					if(value is ObservableObject propValue) {
						observableObjects[prop.Name] = propValue;
						props[prop.Name] = prop;
						disposables[propValue] = ReactiveHelper.RegisterRecursiveObserver(propValue, handler);
					} else if(value is IList list) {
						foreach(object listValue in list) {
							if(listValue is ObservableObject) {
								disposables[(ObservableObject)listValue] = ReactiveHelper.RegisterRecursiveObserver((ObservableObject)listValue, handler);
							}
						}
					}
				}
			}

			PropertyChangedEventHandler? evtHandler = null;

			evtHandler = (s, e) => {
				handler(s, e);

				//Reset change handlers if an object is replaced with another object
				if(e.PropertyName != null && observableObjects.TryGetValue(e.PropertyName, out ObservableObject? obj)) {
					//Remove handlers on the old object
					if(disposables.TryGetValue(obj, out IDisposable? disposable)) {
						disposable.Dispose();
					}

					if(props.TryGetValue(e.PropertyName, out PropertyInfo? prop)) {
						object? value = prop.GetValue(target);
						if(value is ObservableObject propValue) {
							observableObjects[prop.Name] = propValue;

							//Register change handlers on the new object
							disposables[propValue] = ReactiveHelper.RegisterRecursiveObserver(propValue, handler!);
						}
					}
				}
			};

			target.PropertyChanged += evtHandler;

			return new RecursiveObserver(target, evtHandler, disposables.Values);
		}
	}

	public class RecursiveObserver : IDisposable
	{
		private ObservableObject _target;
		private PropertyChangedEventHandler _handler;
		private IEnumerable<IDisposable> _disposables;

		public RecursiveObserver(ObservableObject target, PropertyChangedEventHandler handler, IEnumerable<IDisposable> disposables)
		{
			_target = target;
			_handler = handler;
			_disposables = disposables;
		}

		public void Dispose()
		{
			_target.PropertyChanged -= _handler;
			foreach(IDisposable disposable in _disposables) {
				disposable.Dispose();
			}
		}
	}

	public class DisposableObserver : IDisposable
	{
		private Action DisposeCallback { get; }

		public DisposableObserver(Action dispose)
		{
			DisposeCallback = dispose;
		}

		public void Dispose()
		{
			DisposeCallback();
		}
	}

	public static class ReactiveExtensions
	{
		public static IDisposable ObserveProp(this AvaloniaObject obj, AvaloniaProperty prop, Action<AvaloniaPropertyChangedEventArgs> action)
		{
			return obj.GetPropertyChangedObservable(prop).Subscribe(new AnonymousObserver<AvaloniaPropertyChangedEventArgs>(action));
		}

		public static IDisposable ObserveProp(this INotifyPropertyChanged obj, string property, Action action)
		{
			return ReactiveHelper.RegisterObserver(obj, property, action);
		}

		public static IDisposable ObserveProp(this INotifyPropertyChanged obj, string[] properties, Action action)
		{
			return ReactiveHelper.RegisterObserver(obj, properties, action);
		}
	}
}
