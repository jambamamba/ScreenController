#include "EventHandler.h"

#include <QDebug>

#include "Command.h"
#include "ImageConverterInterface.h"
#include "SocketReader.h"

#if defined(Win32) || defined(Win64)
#include "WindowsMouse.h"
#elif defined(Linux)
#include "X11Mouse.h"
#else
#include "NullMouse.h"
#endif

EventHandler::EventHandler(QObject *parent)
    : QObject(parent)
#if defined(Win32) || defined(Win64)
    ,m_mouse(new WindowsMouse(parent))
#elif defined(Linux)
    ,m_mouse(new X11Mouse(parent))
#else
    ,m_mouse(new NullMouse(parent))
#endif
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
        m_mouse->moveTo(pkt.m_mouse_x, pkt.m_mouse_y);
        break;
    case Command::EventType::MousePress:
        m_mouse->mousePress(pkt.m_mouse_button, pkt.m_mouse_x, pkt.m_mouse_y);
        break;
    case Command::EventType::MouseRelease:
        m_mouse->mouseRelease(pkt.m_mouse_button, pkt.m_mouse_x, pkt.m_mouse_y);
        break;
    case Command::EventType::None:
        break;
    default:
        qDebug() << "recvd unhandled command from " << SocketReader::IpToString(ip) << ", event " << pkt.m_event;
        break;
    }
}
