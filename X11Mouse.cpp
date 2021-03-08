//xinput --test-xi2 --root
#include "X11Mouse.h"
#include <QDebug>

#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XTest.h>
#include <QtX11Extras/QX11Info>

//extern "C" {
//#include "xdotool/xdo.h"
//}

namespace  {
// Get mouse coordinates
void coords(Display *display, int *x, int *y)
{
  XEvent event;
  XQueryPointer (display, DefaultRootWindow (display),
                 &event.xbutton.root, &event.xbutton.window,
                 &event.xbutton.x_root, &event.xbutton.y_root,
                 &event.xbutton.x, &event.xbutton.y,
                 &event.xbutton.state);
  *x = event.xbutton.x;
  *y = event.xbutton.y;
}

}//namespace

X11Mouse::X11Mouse(QObject *parent)
    : MouseInterface(parent)
    , m_display(XOpenDisplay(0))
{
}

X11Mouse::~X11Mouse()
{
    XCloseDisplay(m_display);
}


QImage X11Mouse::getMouseCursor(QPoint &pos) const
{
    QImage cursor;
    XFixesCursorImage *xfcursorImage = XFixesGetCursorImage(m_display);

    static quint32* buf = nullptr;
    static long len = 0;

    long leni = xfcursorImage->width * xfcursorImage->height;
    if(len != leni)
    {
        len = leni;
        delete [] buf;
        buf = new quint32[len];
    }

    for(long i = 0; i < len; ++i)
    {
        buf[i] = (quint32)xfcursorImage->pixels[i];
    }
    cursor = QImage((uchar*)buf,
                    xfcursorImage->width,
                    xfcursorImage->height,
                    QImage::Format_ARGB32_Premultiplied);
    pos = QPoint(xfcursorImage->x, xfcursorImage->y);
    pos -= QPoint(xfcursorImage->xhot, xfcursorImage->yhot);

    XFree(xfcursorImage);

//    cursor.save("/tmp/xcursor.png");
    return cursor;
}

//https://stackoverflow.com/questions/20595716/control-mouse-by-writing-to-dev-input-mice
//mouseClick(Button1)
void X11Mouse::mousePress(int button, int x, int y)
{
//    mouseClick(button, ButtonPress, x, y);
    XTestFakeButtonEvent(m_display, button, true, CurrentTime);
    XFlush(m_display);
}

void X11Mouse::mouseRelease(int button, int x, int y)
{
//    mouseClick(button, ButtonRelease, x, y);
    XTestFakeButtonEvent(m_display, button, false, CurrentTime);
    XFlush(m_display);
}

void X11Mouse::mouseClick(int button, int press_or_release, int, int)
{
    XEvent event;
         memset(&event, 0x00, sizeof(event));

        event.type = press_or_release;
        event.xbutton.button = button;
        event.xbutton.same_screen = True;

        XQueryPointer(m_display, RootWindow(m_display, DefaultScreen(m_display)), &event.xbutton.root, &event.xbutton.window, &event.xbutton.x_root, &event.xbutton.y_root, &event.xbutton.x, &event.xbutton.y, &event.xbutton.state);

        event.xbutton.subwindow = event.xbutton.window;

        while(event.xbutton.subwindow)
        {
            event.xbutton.window = event.xbutton.subwindow;

            XQueryPointer(m_display, event.xbutton.window, &event.xbutton.root, &event.xbutton.subwindow, &event.xbutton.x_root, &event.xbutton.y_root, &event.xbutton.x, &event.xbutton.y, &event.xbutton.state);
        }

        if(XSendEvent(m_display, PointerWindow, True, 0xfff, &event) == 0) fprintf(stderr, "Errore nell'invio dell'evento !!!\n");

        XFlush(m_display);
//    XEvent event;
//    memset(&event, 0x00, sizeof(event));

//    event.type = press_or_release;
//    event.xbutton.button = button;
//    event.xbutton.same_screen = true;

//    XQueryPointer(m_display,
//                  RootWindow(m_display, DefaultScreen(m_display)),
//                  &event.xbutton.root,
//                  &event.xbutton.window,
//                  &event.xbutton.x_root,
//                  &event.xbutton.y_root,
//                  &event.xbutton.x,
//                  &event.xbutton.y,
//                  &event.xbutton.state);
//    event.xbutton.subwindow = event.xbutton.window;
//    event.xbutton.x_root = 0;
//    event.xbutton.y_root = 0;
//    event.xbutton.x = x;
//    event.xbutton.y = y;

//    while(event.xbutton.subwindow)
//    {
//        event.xbutton.window = event.xbutton.subwindow;

//        XQueryPointer(m_display,
//                      event.xbutton.window,
//                      &event.xbutton.root,
//                      &event.xbutton.subwindow,
//                      &event.xbutton.x_root,
//                      &event.xbutton.y_root,
//                      &event.xbutton.x,
//                      &event.xbutton.y,
//                      &event.xbutton.state);
//    }

    qDebug() << ((press_or_release == ButtonPress) ? "mousePress" : "mouseRelease")
             << "button" << button
             << "@"
             << event.xbutton.x
             << ","
             << event.xbutton.y;

//    int ret = XSendEvent(m_display, PointerWindow, false, 0xfff, &event);
//    if(ret == 0)
//    {qDebug() << "Error" << errno << strerror(errno);}
//    XFlush(m_display);
}

void X11Mouse::moveTo(int x, int y)
{
    qDebug() << "mouseTo"
             << x
             << ","
             << y;

    XSelectInput(m_display, DefaultRootWindow (m_display), KeyReleaseMask);
    XWarpPointer(m_display, None, DefaultRootWindow (m_display), 0, 0, 0, 0, x, y);
    XFlush(m_display);

}
