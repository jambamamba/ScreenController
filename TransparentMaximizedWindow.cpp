#include "TransparentMaximizedWindow.h"
#include "ui_TransparentMaximizedWindow.h"

#if defined(Win32) || defined(Win64)
#include <windows.h>
#endif

#include <QScreen>
#include <QDesktopWidget>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QLabel>
#include <QSizeGrip>

static const float WINDOW_OPACITY = 0.5f;

TransparentMaximizedWindow::TransparentMaximizedWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TransparentMaximizedWindow),
    m_capturing(false)
{
    ui->setupUi(this);
}

TransparentMaximizedWindow::~TransparentMaximizedWindow()
{
    delete ui;
}

void TransparentMaximizedWindow::moveToScreen(const QScreen* screen)
{
    QRect screen_geometry = screen->geometry();
    move(screen_geometry.x(), screen_geometry.y());
    resize(screen_geometry.width(), screen_geometry.height());
}

void TransparentMaximizedWindow::show(int width, int height, QScreen* screen)
{
    m_screen = screen;
    moveToScreen(screen);
    m_capturing = false;
    setWindowState(Qt::WindowFullScreen);
    setWindowFlags(Qt::Window
                   | Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::X11BypassWindowManagerHint
                   );
#ifdef Linux
    setAttribute(Qt::WA_TranslucentBackground, true);
#elif defined(Win32) || defined(Win64)
    setWindowOpacity(WINDOW_OPACITY);
#endif
    setFixedSize(width, height);
    QWidget::showFullScreen();
}

void TransparentMaximizedWindow::mousePressEvent(QMouseEvent *mouse_event)
{
    if(m_capturing) {return;}
}

void TransparentMaximizedWindow::mouseMoveEvent(QMouseEvent *mouse_event)
{
    if(m_capturing) {return;}
}

void TransparentMaximizedWindow::mouseReleaseEvent(QMouseEvent *mouse_event)
{
    if(m_capturing) {return;}
}

void TransparentMaximizedWindow::startCapture(
        const QPoint &point_start,
        const QPoint &point_stop)
{
    m_capturing = true;
#if defined(Win32) || defined(Win64)
    HWND hwnd = (HWND) winId();
    LONG styles = GetWindowLong(hwnd, GWL_EXSTYLE);
    SetWindowLong(hwnd, GWL_EXSTYLE, styles | WS_EX_TRANSPARENT);
#elif defined Linux
//    setWindowFlags(windowFlags() | Qt::WindowTransparentForInput);
//    setAttribute(Qt::WA_TransparentForMouseEvents);
#elif defined Darwin
    setIgnoresMouseEvents(true);
#endif
    qDebug() << "startCapture";

    update();
}


