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

#include "Command.h"

static const float WINDOW_OPACITY = 0.5f;

namespace  {
Command CreateKeyCommandPacket(Command::EventType event_type, const QKeyEvent *event)
{
    Command pkt;
    pkt.m_event = event_type;
    pkt.m_key = event->key();
    if(event->modifiers().testFlag(Qt::ShiftModifier))
    { pkt.m_key_modifier |= Qt::ShiftModifier; }
    else if(event->modifiers().testFlag(Qt::ControlModifier))
    { pkt.m_key_modifier |= Qt::ControlModifier; }
    else if(event->modifiers().testFlag(Qt::AltModifier))
    { pkt.m_key_modifier |= Qt::AltModifier; }
    else if(event->modifiers().testFlag(Qt::KeypadModifier))
    { pkt.m_key_modifier |= Qt::KeypadModifier; }
    else if(event->modifiers().testFlag(Qt::GroupSwitchModifier))
    { pkt.m_key_modifier |= Qt::GroupSwitchModifier; }
    else if(event->modifiers().testFlag(Qt::KeyboardModifierMask))
    { pkt.m_key_modifier |= Qt::KeyboardModifierMask; }

    return pkt;
}

Command CreateMouseCommandPacket(Command::EventType event_type, const QMouseEvent *event)
{
    Command pkt;
    pkt.m_event = event_type;
    pkt.m_mouse_button = event->button();
    pkt.m_mouse_x = event->pos().x();
    pkt.m_mouse_y = event->pos().y();

    return pkt;
}
}//namespace

TransparentMaximizedWindow::TransparentMaximizedWindow(const QString &ip, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TransparentMaximizedWindow)
    , m_ip(ip)
    , m_capturing(false)
    , m_timer(new QTimer(this))
{
    ui->setupUi(this);

//    installEventFilter(this);
//    grabKeyboard();
//    setFocusPolicy(Qt::ClickFocus);

    connect(m_timer, &QTimer::timeout, [this](){
        repaint(rect());
    });
    m_timer->start(200);
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
//    static int counter = 0;
//    qDebug() << "set image " << counter++;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_image = img;
    }
//    while(QApplication::hasPendingEvents())
//    {
//        QApplication::processEvents();
    //    }
}

bool TransparentMaximizedWindow::IsClosed() const
{
    return m_closed;
}

void TransparentMaximizedWindow::ReOpen()
{
 m_closed = false;
}

void TransparentMaximizedWindow::keyPressEvent(QKeyEvent *event)
{
    if((event->key() == 'q' || event->key() == 'Q') &&
            (event->modifiers().testFlag(Qt::AltModifier)))
    {
        m_closed = true;
        emit Close();
//        exit(0);//todo
    }
    auto pkt = CreateKeyCommandPacket(Command::EventType::KeyPress, event);
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::keyReleaseEvent(QKeyEvent *event)
{
    auto pkt = CreateKeyCommandPacket(Command::EventType::KeyRelease, event);
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::mousePressEvent(QMouseEvent *event)
{
    qDebug() << "TransparentMaximizedWindow::mousePressEvent";
    auto pkt = CreateMouseCommandPacket(Command::EventType::MouseMove, event);
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::mouseReleaseEvent(QMouseEvent *event)
{
    qDebug() << "TransparentMaximizedWindow::mouseReleaseEvent";
    auto pkt = CreateMouseCommandPacket(Command::EventType::MouseRelease, event);
    emit SendCommandToNode(pkt);
}

void TransparentMaximizedWindow::mouseMoveEvent(QMouseEvent *event)
{
    qDebug() << "TransparentMaximizedWindow::mouseMoveEvent";
    auto pkt = CreateMouseCommandPacket(Command::EventType::MouseMove, event);
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


