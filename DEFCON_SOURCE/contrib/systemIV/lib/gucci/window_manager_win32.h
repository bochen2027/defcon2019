#ifndef INCLUDED_WINDOW_MANAGER_WIN32_H
#define INCLUDED_WINDOW_MANAGER_WIN32_H

#include "window_manager.h"

#define NOMINMAX
#include <windows.h>

class WindowManagerWin32 : public WindowManager
{
public:
	WindowManagerWin32();

	bool        CreateWin           (int _width, int _height,		                        // Set _colourDepth, _refreshRate and/or 
			                         bool _windowed, int _colourDepth,		                // _zDepth to -1 to get default values
			                         int _refreshRate, int _zDepth,
									 int _antiAlias,
		                             const char *_title );
    
	void		HideWin				();
	void        DestroyWin          ();
	void        Flip                ();
	void        PollForMessages     ();
	void        SetMousePos         (int x, int y);
    
	void        CaptureMouse        ();
	void        UncaptureMouse      ();
    
	void        HideMousePointer    ();
	void        UnhideMousePointer  ();
    
    void        SaveDesktop         ();
    void        RestoreDesktop      ();
    void        OpenWebsite			( const char *_url );
    HINSTANCE   GethInstance();

public:
	HWND		m_hWnd;
	HDC			m_hDC;
	HGLRC		m_hRC;

protected:
	void        ListAllDisplayModes ();
	void        EnableOpenGL        (int _colourDepth, int _zDepth);
	void        DisableOpenGL       ();
};

inline WindowManagerWin32 *GetWindowManagerWin32()
{
    return static_cast<WindowManagerWin32 *>(g_windowManager);
}

#endif
