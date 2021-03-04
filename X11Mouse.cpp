#include "X11Mouse.h"
#include <QDebug>

#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xfixes.h>
#include <QtX11Extras/QX11Info>

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
void X11Mouse::mouseClick(int button)
{
    XEvent event;
    memset(&event, 0x00, sizeof(event));

    event.type = ButtonPress;
    event.xbutton.button = button;
    event.xbutton.same_screen = true;

    XQueryPointer(m_display, RootWindow(m_display, DefaultScreen(m_display)), &event.xbutton.root, &event.xbutton.window, &event.xbutton.x_root, &event.xbutton.y_root, &event.xbutton.x, &event.xbutton.y, &event.xbutton.state);

    event.xbutton.subwindow = event.xbutton.window;

    while(event.xbutton.subwindow)
    {
        event.xbutton.window = event.xbutton.subwindow;

        XQueryPointer(m_display, event.xbutton.window, &event.xbutton.root, &event.xbutton.subwindow, &event.xbutton.x_root, &event.xbutton.y_root, &event.xbutton.x, &event.xbutton.y, &event.xbutton.state);
    }

    if(XSendEvent(m_display, PointerWindow, true, 0xfff, &event) == 0) {fprintf(stderr, "Error\n");}

    XFlush(m_display);

    usleep(1000 * 100);

    event.type = ButtonRelease;
    event.xbutton.state = 0x100;

    if(XSendEvent(m_display, PointerWindow, true, 0xfff, &event) == 0) {fprintf(stderr, "Error\n");}

    XFlush(m_display);

    XCloseDisplay(m_display);
}

// Move mouse pointer (absolute)
void X11Mouse::moveTo(int x, int y)
{
  int cur_x, cur_y;
  coords (m_display, &cur_x, &cur_y);
//  XWarpPointer (m_display, None, None, 0,0,0,0, -cur_x, -cur_y);
  XWarpPointer (m_display, None, None, 0,0,0,0, x, y);
  usleep(1000 * 100);
}
