#pragma once

#include <qnamespace.h>
#include <stdint.h>

#include <QObject>

struct Command
{
    uint8_t m_sync_bytes[4] = { 0xca, 0xfe, 0xba, 0xbe };
    uint8_t m_version = 1;
    uint8_t m_padding = 0;
    enum EventType : int {
        None,
        StartStreaming,
        StopStreaming,
        StoppedStreaming,
        MousePress,
        MouseRelease,
        MouseMove,
        KeyPress,
        KeyRelease
    };
    uint16_t m_event = None;
    uint32_t m_size = sizeof (Command);
    uint32_t m_key_modifier = Qt::KeyboardModifier::NoModifier;
    int m_key = -1;
    int m_mouse_button = Qt::MouseButton::NoButton;
    int m_mouse_x = -1;
    int m_mouse_y = -1;
    uint8_t m_tail_bytes[4] = { 0xca, 0xfe, 0xd0, 0x0d };

    Command(uint8_t version = 1, uint16_t cmd_id = 0);
};
Q_DECLARE_METATYPE(Command);
