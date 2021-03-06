#include "TransparentMaximizedWindow.h"
#include "ui_TransparentMaximizedWindow.h"

#if defined(Win32) || defined(Win64)
#include <windows.h>
#endif

#include <QtCore/qnamespace.h>
#include <QScreen>
#include <QDesktopWidget>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QLabel>
#include <QSizeGrip>
#include <QTimer>
#include <xkbcommon/xkbcommon-keysyms.h>

#if defined(Win32) || defined(Win64)
#include "WindowsMouse.h"
#elif defined(Linux)
#include "X11Mouse.h"
#include "X11Key.h"
#else
#include "NullMouse.h"
#endif

static const float WINDOW_OPACITY = 0.5f;

namespace  {
Command CreateKeyCommandPacket(uint32_t key, uint32_t modifier, uint32_t type)
{
    Command pkt;
    pkt.m_event = Command::EventType::KeyEvent;
    pkt.u.m_key.m_code = key;
    pkt.u.m_key.m_modifier = modifier;
    pkt.u.m_key.m_type = type;

    return pkt;
}

Command CreateMouseCommandPacket(Command::EventType event_type, const QMouseEvent *event)
{
    Command pkt;
    pkt.m_event = event_type;
    pkt.u.m_mouse.m_button = event->button();
    pkt.u.m_mouse.m_x = event->globalX();
    pkt.u.m_mouse.m_y = event->globalY();

    return pkt;
}
}//namespace

TransparentMaximizedWindow::TransparentMaximizedWindow(const QString &ip, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TransparentMaximizedWindow)
    , m_ip(ip)
    , m_capturing(false)
    , m_timer(new QTimer(this))
#if defined(Win32) || defined(Win64)
    ,m_mouse(std::make_unique<WindowsMouse>(parent))
    ,m_key(std::make_unique<WindowsKey>(parent))
#elif defined(Linux)
    ,m_mouse(std::make_unique<X11Mouse>(parent))
    ,m_key(std::make_unique<X11Key>(parent))
#else
    ,m_mouse(std::make_unique<NullMouse>(parent))
    ,m_key(std::make_unique<NullKey>(parent))
#endif
{
    ui->setupUi(this);

    for(int i = 0; i < DebounceEvents::TotalEvents; ++i)
    {
        m_debounce_interval[i] = std::chrono::steady_clock::now();
    }

//    installEventFilter(this);
//    grabKeyboard();
//    setFocusPolicy(Qt::ClickFocus);

    connect(m_timer, &QTimer::timeout, [this](){
        repaint(rect());
    });
    m_timer->start(200);

    m_event_capture_thread = std::async([this](){
        while(!m_die)
        {
            uint32_t key = 0;
            uint32_t modifier = 0;
            uint32_t type = 0;
            if(m_key->testKeyEvent(winId(), key, modifier, type))
            {
                emit SendCommandToNode(CreateKeyCommandPacket(key, modifier, type));

                if((m_key->testKey('q', key) &&
                    m_key->testAltModifier(modifier)))
                {
                    m_die = true;
                    emit Close();
                }
            }
        }
    });
}

TransparentMaximizedWindow::~TransparentMaximizedWindow()
{
    m_event_capture_thread.wait();
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
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_image = img;
    }
}

bool TransparentMaximizedWindow::IsClosed() const
{
    return m_die;
}

void TransparentMaximizedWindow::ReOpen()
{
    show();
    m_die = false;
}

bool TransparentMaximizedWindow::Debounce(DebounceEvents event, int *out_elapsed)
{
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - m_debounce_interval[event]).count();
    m_debounce_interval[event] = t1;
    if(out_elapsed) { *out_elapsed = elapsed; }
    return elapsed < 100;
}

#if 0
void TransparentMaximizedWindow::keyPressEvent(QKeyEvent *event)
{
    if((event->key() == 'q' || event->key() == 'Q') &&
            (event->modifiers().testFlag(Qt::AltModifier)))
    {
        m_die = true;
        emit Close();
    }
    int elapsed = 0;
    if(Debounce(DebounceEvents::KeyPress, &elapsed))
    { return; }

    auto pkt = CreateKeyCommandPacket(Command::EventType::KeyPress, event);
    qDebug() << "keyPress key:" << pkt.m_key << "modifier:" << pkt.m_modifier;
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::keyReleaseEvent(QKeyEvent *event)
{
    if(Debounce(DebounceEvents::KeyRelease))
    { return; }

    auto pkt = CreateKeyCommandPacket(Command::EventType::KeyRelease, event);
    emit SendCommandToNode(pkt);
}
#endif//0

void TransparentMaximizedWindow::mousePressEvent(QMouseEvent *event)
{
    if(m_mouse_button_state == DebounceEvents::MousePress) { return; }

    m_mouse_button_state = DebounceEvents::MousePress;
    auto pkt = CreateMouseCommandPacket(Command::EventType::MousePress, event);
    pkt.u.m_mouse.m_x = m_mouse_pos.x();
    pkt.u.m_mouse.m_y = m_mouse_pos.y();
    qDebug() << "mousePress @ " << pkt.u.m_mouse.m_x << "," << pkt.u.m_mouse.m_y;
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if(m_mouse_button_state == DebounceEvents::MouseRelease) { return; }

    m_mouse_button_state = DebounceEvents::MouseRelease;
    auto pkt = CreateMouseCommandPacket(Command::EventType::MouseRelease, event);
    pkt.u.m_mouse.m_x = m_mouse_pos.x();
    pkt.u.m_mouse.m_y = m_mouse_pos.y();
    qDebug() << "mouseRelease @ " << pkt.u.m_mouse.m_x << "," << pkt.u.m_mouse.m_y;
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::mouseMoveEvent(QMouseEvent *event)
{
    auto pkt = CreateMouseCommandPacket(Command::EventType::MouseMove, event);
    qDebug() << "mouseMove @ " << pkt.u.m_mouse.m_x << "," << pkt.u.m_mouse.m_y;
    m_mouse_pos = QPoint(pkt.u.m_mouse.m_x, pkt.u.m_mouse.m_y);
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    auto pkt = CreateMouseCommandPacket(Command::EventType::MouseDoubleClicked, event);
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::Show(int width, int height, QScreen* screen)
{
    m_screen = screen;
    MoveToScreen(screen);
    m_capturing = false;
    setMouseTracking(true);//needed for mouseMove
//    setWindowState(Qt::WindowFullScreen);
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

//bool TransparentMaximizedWindow::eventFilter(QObject *obj, QEvent *event)
//{
//    if(obj == this)
//    {
//        {
//            auto ev = static_cast<QKeyEvent *>(event);
//            if ( ev->type() == QEvent::KeyPress)
//            {
//                if((ev->key() == 'q' || ev->key() == 'Q') &&
//                        (ev->modifiers() == Qt::AltModifier))
//                {
//                    emit Close();
//                    exit(0);
//                }
//            }
//        }
//    }
//    //if you want to filter the event out, i.e. stop it being handled further, return true;
//    return QWidget::eventFilter(obj, event);
//}

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


