#pragma once
#include "Linux/include/X11Lib.h"
#include "Shared/Interfaces/IMouseManager.h"

class LinuxMouseManager : public IMouseManager
{
private:
	Window _mainWindow = {};

	Display* _display = {};
	int _defaultScreen = 0;
	Window _rootWindow = {};

	Cursor _defaultCursor = {};
	Cursor _crossCursor = {};
	Cursor _hiddenCursor = {};

public:
	LinuxMouseManager(void* windowHandle);
	virtual ~LinuxMouseManager();

	SystemMouseState GetSystemMouseState(void* rendererHandle);
	bool CaptureMouse(int32_t x, int32_t y, int32_t width, int32_t height, void* rendererHandle);
	void ReleaseMouse();
	void SetSystemMousePosition(int32_t x, int32_t y);
	void SetCursorImage(CursorImage cursor);
	double GetPixelScale();
};
