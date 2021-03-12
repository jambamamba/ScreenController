#pragma once

#include <qnamespace.h>
#include <stdint.h>

#include <QObject>

struct Command
{
    uint8_t m_sync_bytes[4] = { 0xca, 0xfe, 0xba, 0xbe };
    uint8_t m_version = 1;
    uint8_t m_padding = 0;
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
        ssize_t m_x = 0;
        ssize_t m_y = 0;
        ssize_t m_width = 0;
        ssize_t m_height = 0;
        ssize_t m_screen_width = 0;
        ssize_t m_screen_height = 0;
        ssize_t m_size = 0;
        size_t m_decoder_type = 0;
    };

    union U {
        Key m_key;
        Mouse m_mouse;
        Frame m_frame;
        U(){}
    } u;
    uint8_t m_tail_bytes[4] = { 0xca, 0xfe, 0xd0, 0x0d };

    Command(uint8_t version = 1, uint16_t cmd_id = 0);
};
Q_DECLARE_METATYPE(Command);
