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
#include <QTimer>

static const float WINDOW_OPACITY = 0.5f;

TransparentMaximizedWindow::TransparentMaximizedWindow(const QString &ip, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TransparentMaximizedWindow)
    , m_ip(ip)
    , m_capturing(false)
    , m_timer(new QTimer(this))
{
    ui->setupUi(this);
//    void repaint(const QRect &)

    connect(m_timer, &QTimer::timeout, [this](){
        repaint(rect());
    });
    m_timer->start(100);

    installEventFilter(this);
    grabKeyboard();
    setFocusPolicy(Qt::ClickFocus);
}

TransparentMaximizedWindow::~TransparentMaximizedWindow()
{
    delete ui;
}

void TransparentMaximizedWindow::MoveToScreen(const QScreen* screen)
{
    QRect screen_geometry = screen->geometry();
    move(screen_geometry.x(), screen_geometry.y());
    resize(screen_geometry.width(), screen_geometry.height());
}

void TransparentMaximizedWindow::SetImage(const QImage &img)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    m_image = img;
}

void TransparentMaximizedWindow::Show(int width, int height, QScreen* screen)
{
    m_screen = screen;
    MoveToScreen(screen);
    m_capturing = false;
    setWindowState(Qt::WindowFullScreen);
    setWindowFlags(Qt::Window
                   | Qt::FramelessWindowHint
//                   | Qt::WindowStaysOnTopHint
//                   | Qt::X11BypassWindowManagerHint
                   );
#ifdef Linux
    setAttribute(Qt::WA_TranslucentBackground, true);
#elif defined(Win32) || defined(Win64)
    setWindowOpacity(WINDOW_OPACITY);
#endif
    setFixedSize(width, height);
    QWidget::showFullScreen();
}

bool TransparentMaximizedWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(obj == this)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if ( keyEvent->type() == QEvent::KeyPress)
        {
            if((keyEvent->key() == 'q' || keyEvent->key() == 'Q') &&
                    (keyEvent->modifiers() == Qt::AltModifier))
            {
                emit Close();
            }
        }
    }
    return true;
}

void TransparentMaximizedWindow::paintEvent(QPaintEvent *)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
//    painter.setPen(QPen(Qt::green, BORDER_WIDTH, Qt::DashDotLine, Qt::FlatCap, Qt::MiterJoin));
//    QRect r(rect);
//    r.setX(rect.x() - BORDER_WIDTH);
//    r.setY(rect.y() - BORDER_WIDTH);
//    r.setWidth(rect.width() + BORDER_WIDTH*2);
//    r.setHeight(rect.height() + BORDER_WIDTH*2);
//    painter.drawRect(r);
    painter.drawImage(rect(), m_image, m_image.rect());
    painter.end();

}


void TransparentMaximizedWindow::StartCapture(
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


