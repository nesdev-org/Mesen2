#pragma once

#include <stdint.h>
#include <dlfcn.h>

typedef unsigned long XID;
typedef XID Window;
typedef XID Cursor;
typedef XID Pixmap;
typedef XID Drawable;
typedef struct _XDisplay Display;
typedef int (*XErrorHandler)(Display*, void*);

typedef struct
{
	unsigned long pixel;
	unsigned short red, green, blue;
	char flags;
	char pad;
} XColor;

#define XC_crosshair 34
#define XC_left_ptr 68
#define NoEventMask 0L
#define GrabModeAsync 1

typedef Display* (*PFN_XOpenDisplay)(const char*);
typedef int (*PFN_XDefaultScreen)(Display*);
typedef Window (*PFN_XRootWindow)(Display*, int);
typedef Cursor (*PFN_XCreateFontCursor)(Display*, unsigned int);
typedef Pixmap (*PFN_XCreateBitmapFromData)(Display*, Drawable, const char*, unsigned int, unsigned int);
typedef Cursor (*PFN_XCreatePixmapCursor)(Display*, Pixmap, Pixmap, XColor*, XColor*, unsigned int, unsigned int);
typedef int (*PFN_XGrabServer)(Display*);
typedef int (*PFN_XUngrabServer)(Display*);
typedef int (*PFN_XFlush)(Display*);
typedef int (*PFN_XQueryPointer)(Display*, Window, Window*, Window*, int*, int*, int*, int*, unsigned int*);
typedef int (*PFN_XGrabPointer)(Display*, Window, int, unsigned int, int, int, Window, Cursor, unsigned long);
typedef int (*PFN_XUngrabPointer)(Display*, unsigned long);
typedef int (*PFN_XWarpPointer)(Display*, Window, Window, int, int, unsigned int, unsigned int, int, int);
typedef int (*PFN_XDefineCursor)(Display*, Window, Cursor);

inline PFN_XOpenDisplay ptr_XOpenDisplay = nullptr;
inline PFN_XDefaultScreen ptr_XDefaultScreen = nullptr;
inline PFN_XRootWindow ptr_XRootWindow = nullptr;
inline PFN_XCreateFontCursor ptr_XCreateFontCursor = nullptr;
inline PFN_XCreateBitmapFromData ptr_XCreateBitmapFromData = nullptr;
inline PFN_XCreatePixmapCursor ptr_XCreatePixmapCursor = nullptr;
inline PFN_XGrabServer ptr_XGrabServer = nullptr;
inline PFN_XUngrabServer ptr_XUngrabServer = nullptr;
inline PFN_XFlush ptr_XFlush = nullptr;
inline PFN_XQueryPointer ptr_XQueryPointer = nullptr;
inline PFN_XGrabPointer ptr_XGrabPointer = nullptr;
inline PFN_XUngrabPointer ptr_XUngrabPointer = nullptr;
inline PFN_XWarpPointer ptr_XWarpPointer = nullptr;
inline PFN_XDefineCursor ptr_XDefineCursor = nullptr;

#define XOpenDisplay ptr_XOpenDisplay
#define XDefaultScreen ptr_XDefaultScreen
#define XRootWindow ptr_XRootWindow
#define XCreateFontCursor ptr_XCreateFontCursor
#define XCreateBitmapFromData ptr_XCreateBitmapFromData
#define XCreatePixmapCursor ptr_XCreatePixmapCursor
#define XGrabServer ptr_XGrabServer
#define XUngrabServer ptr_XUngrabServer
#define XFlush ptr_XFlush
#define XQueryPointer ptr_XQueryPointer
#define XGrabPointer ptr_XGrabPointer
#define XUngrabPointer ptr_XUngrabPointer
#define XWarpPointer ptr_XWarpPointer
#define XDefineCursor ptr_XDefineCursor

inline bool LoadX11(void)
{
	static bool loaded = false;
	if(loaded) {
		return true;
	}

	void* handle = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
	if(!handle) {
		handle = dlopen("libX11.so", RTLD_LAZY | RTLD_LOCAL);
	}
	if(!handle) {
		return false;
	}

#define LOAD_SYMBOL(type, name) \
	ptr_##name = (type)dlsym(handle, #name); \
	if (!ptr_##name) { return false; }

	LOAD_SYMBOL(PFN_XOpenDisplay, XOpenDisplay);
	LOAD_SYMBOL(PFN_XDefaultScreen, XDefaultScreen);
	LOAD_SYMBOL(PFN_XRootWindow, XRootWindow);
	LOAD_SYMBOL(PFN_XCreateFontCursor, XCreateFontCursor);
	LOAD_SYMBOL(PFN_XCreateBitmapFromData, XCreateBitmapFromData);
	LOAD_SYMBOL(PFN_XCreatePixmapCursor, XCreatePixmapCursor);
	LOAD_SYMBOL(PFN_XGrabServer, XGrabServer);
	LOAD_SYMBOL(PFN_XUngrabServer, XUngrabServer);
	LOAD_SYMBOL(PFN_XFlush, XFlush);
	LOAD_SYMBOL(PFN_XQueryPointer, XQueryPointer);
	LOAD_SYMBOL(PFN_XGrabPointer, XGrabPointer);
	LOAD_SYMBOL(PFN_XUngrabPointer, XUngrabPointer);
	LOAD_SYMBOL(PFN_XWarpPointer, XWarpPointer);
	LOAD_SYMBOL(PFN_XDefineCursor, XDefineCursor);

#undef LOAD_SYMBOL

	loaded = true;
	return true;
}