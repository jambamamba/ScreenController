#include "EventHandler.h"

#include <QDebug>

#include "Command.h"
#include "ImageConverterInterface.h"
#include "SocketReader.h"

#if defined(Win32) || defined(Win64)
#include "WindowsMouse.h"
#elif defined(Linux)
#include "X11Mouse.h"
#include "X11Key.h"
#else
#include "NullMouse.h"
#endif

EventHandler::EventHandler(QObject *parent)
    : QObject(parent)
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
{}

EventHandler::~EventHandler()
{}

void EventHandler::HandleCommand(const Command &pkt, uint32_t ip)
{
    switch(pkt.m_event)
    {
    case Command::EventType::StartStreaming:
        emit StartStreaming(ip, (int)ImageConverterInterface::Types::Webp);
        break;
    case Command::EventType::StopStreaming:
        emit StopStreaming(ip);
        break;
    case Command::EventType::StoppedStreaming:
        emit StoppedStreaming(ip);
        break;
    case Command::EventType::MouseMove:
        m_mouse->moveTo(pkt.u.m_mouse.m_x, pkt.u.m_mouse.m_y);
        break;
    case Command::EventType::MousePress:
        m_mouse->buttonPress(pkt.u.m_mouse.m_button, pkt.u.m_mouse.m_x, pkt.u.m_mouse.m_y);
        break;
    case Command::EventType::MouseRelease:
        m_mouse->buttonRelease(pkt.u.m_mouse.m_button, pkt.u.m_mouse.m_x, pkt.u.m_mouse.m_y);
        break;
    case Command::EventType::KeyEvent:
        m_key->keyEvent(pkt.u.m_key.m_code, pkt.u.m_key.m_modifier, pkt.u.m_key.m_type);
        break;
    case Command::EventType::None:
        break;
    default:
        qDebug() << "recvd unhandled command from " << SocketReader::IpToString(ip) << ", event " << pkt.m_event;
        break;
    }
}
