#include "X11Key.h"
#include <QDebug>
#include <QApplication>

#include <QtX11Extras/QX11Info>
#include <X11/Intrinsic.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/Xfixes.h>
#include "/home/dev/oosman/Qt/5/5.15.0/Src/qtvirtualkeyboard/tests/manual/x11vkbwrapper/xcbkeyboard.h"

static bool s_last_x11_error = false;

namespace  {

static int MyX11ErrorHandler(Display *, XErrorEvent *)
{
    qDebug() << "X11 Error";
    s_last_x11_error = true;
}

XKeyEvent createKeyEvent(Display *display, Window &win,
                         Window &winRoot, bool press,
                         int keycode, int modifiers)
{
    XKeyEvent event;

	event.display     = display;
	event.window      = win;
	event.root        = winRoot;
	event.subwindow   = None;
	event.time        = CurrentTime;
	event.x           = 1;
	event.y           = 1;
	event.x_root      = 1;
	event.y_root      = 1;
	event.same_screen = True;
	event.keycode     = XKeysymToKeycode(display, keycode);
	event.state       = modifiers;
	if(press)
		event.type = KeyPress;
	else
		event.type = KeyRelease;

	return event;
}

}//namespace

X11Key::X11Key(QObject *parent)
    : KeyInterface(parent)
    , m_display(XOpenDisplay(0))
    , m_registered_hotkey_count(0)
{
    XSetErrorHandler(MyX11ErrorHandler);
}

X11Key::~X11Key()
{
    XCloseDisplay(m_display);
}

//alternative: https://stackoverflow.com/questions/4037230/global-hotkey-with-x11-xlib
void X11Key::testHotKeyPress()
{
    XEvent      ev;

    XSelectInput(m_display, DefaultRootWindow(m_display), KeyPressMask);
    if(XPending(m_display))
    {
        XNextEvent(m_display, &ev);
        switch(ev.type)
        {
        case KeyPress:
//            qDebug() << "Hot key pressed!"
//                     << "type" << ev.type
//                     << "modifier" << ev.xkey.state
//                     << "key" << XKeycodeToKeysym(m_dpy, ev.xkey.keycode, 0);
            emit hotKeyPressed(XLookupKeysym(&ev.xkey, 0), ev.xkey.state);
            break;
        default:
//            qDebug() << "key press detected"
//                     << "type" << ev.type
//                     << "state" << ev.xkey.state
//                     << "keycode" << ev.xkey.keycode;
            break;
        }
    }

    if(m_registered_hotkey_count > 0)
    {
        QMetaObject::invokeMethod(this, "testHotKeyPress",
                          Qt::QueuedConnection);
    }
}

void X11Key::onRegisterHotKey(quint32 key, quint32 modifiers)
{
    Window      root    = DefaultRootWindow(m_display);

    int             keycode         = XKeysymToKeycode(m_display, key/*XK_K*/);
    Bool            owner_events    = False;
    int             pointer_mode    = GrabModeAsync;
    int             keyboard_mode   = GrabModeAsync;

    s_last_x11_error = false;
    XGrabKey(m_display, keycode, modifiers, root, owner_events, pointer_mode,
             keyboard_mode);
    qDebug() << "onRegisterHotKey" << ((s_last_x11_error) ? "error" : "success")
             << "key" << key
             << "modifiers" << modifiers;
    if(!s_last_x11_error)
    {
        m_registered_hotkey_count++;
    }
    emit registeredHotKey(!s_last_x11_error, key, modifiers);
    if(m_registered_hotkey_count == 1)
    {
        QMetaObject::invokeMethod(this, "testHotKeyPress",
                                  Qt::QueuedConnection);
    }
}

void X11Key::onUnRegisterHotKey(quint32 key, quint32 modifiers)
{
    Window      root    = DefaultRootWindow(m_display);
    int             keycode         = XKeysymToKeycode(m_display, key);

    s_last_x11_error = false;
    XUngrabKey(m_display, keycode, modifiers, root);
    qDebug() << "onUnRegisterHotKey" << ((s_last_x11_error) ? "error" : "success")
             << "key" << key
             << "modifiers" << modifiers;
    if(!s_last_x11_error && m_registered_hotkey_count > 0)
    {
        m_registered_hotkey_count--;
    }
}

void X11Key::keyPress(unsigned int keyCode)
{
	for(size_t i =0; KeyTbl[i] != 0; ++i)
	{
		if(KeyTbl[i] == keyCode)
		{
			keyCode = KeyTbl[i-1];
			break;
		}
	}
	qDebug() << "X11Key::keyPress key:" << keyCode << XStringToKeysym("a");
	keyCode = XStringToKeysym("a");

	// Get the root window for the current display.
		Window winRoot = XDefaultRootWindow(m_display);

	// Find the window which has the current keyboard focus.
		Window winFocus;
		int    revert;
		XGetInputFocus(m_display, &winFocus, &revert);

	// Send a fake key press event to the window.
		XKeyEvent event = createKeyEvent(m_display, winFocus, winRoot, true, keyCode, 0);
		XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);

	// Send a fake key release event to the window.
		event = createKeyEvent(m_display, winFocus, winRoot, false, keyCode, 0);
		XSendEvent(event.display, event.window, True, KeyPressMask, (XEvent *)&event);

	XFlush(m_display);
}
