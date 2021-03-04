#include "EventHandler.h"

#include <QDebug>

#include "Command.h"
#include "ImageConverterInterface.h"

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
    case Command::EventType::MouseMove:
        m_mouse->moveTo(pkt.m_mouse_x, pkt.m_mouse_y);
        break;
    default:
        //todo
        qDebug() << "recvd command from " << ip << ", event " << pkt.m_event;
        break;
    }
}
