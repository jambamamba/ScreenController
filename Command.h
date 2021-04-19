#pragma once

#include <qnamespace.h>
#include <stdint.h>

#include <QObject>

struct Command
{
    uint8_t m_sync_bytes[4] = { 0xca, 0xfe, 0xba, 0xbe };
    uint8_t m_version = 1;
    uint8_t m_padding[0];
    enum EventType : uint16_t {
        None,
        StartStreaming,
        StopStreaming,
        StoppedStreaming,
        MousePress,
        MouseRelease,
        MouseDoubleClicked,
        MouseMove,
        KeyEvent,
        FrameInfo,
    };
    uint16_t m_event = None;
    uint32_t m_size = sizeof (Command);
    struct Key {
        uint32_t m_code = 0;
        uint32_t m_modifier = 0;
        uint32_t m_type = 0;
    };
    struct Mouse {
        uint32_t m_button = 0;
        uint32_t m_x = -1;
        uint32_t m_y = -1;
    };
    struct Frame {
        uint32_t width = 0;
        uint32_t height = 0;
    };
    union U {
        Key _key;
        Mouse _mouse;
        Frame _frame;
        U(){}
    } u;
    uint8_t m_tail_bytes[4] = { 0xca, 0xfe, 0xd0, 0x0d };

    Command(uint16_t event = 0);

};

Q_DECLARE_METATYPE(Command);
