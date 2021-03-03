#pragma once

#include "ImageConverterInterface.h"

struct CommandMessage : public ImageConverterInterface
{
    struct Packet
    {
        uint8_t m_sync_bytes[4] = { 0xca, 0xfe, 0xba, 0xbe };
        uint8_t m_version;
        uint8_t m_padding;
        enum EventType : int {
            None,
            StartStreaming,
            StopStreaming,
            MousePress,
            MouseRelease,
            MouseMove,
            KeyPress,
            KeyRelease
        };
        uint16_t m_event;
        uint32_t m_size;
        uint32_t m_key_modifier = Qt::KeyboardModifier::NoModifier;
        int m_key = -1;
        int m_mouse_button = Qt::MouseButton::NoButton;
        int m_mouse_x = -1;
        int m_mouse_y = -1;
        uint8_t m_tail_bytes[4] = { 0xca, 0xfe, 0xd0, 0x0d };

        Packet(uint8_t version = 1, uint16_t cmd_id = 0)
            : m_version(version)
            , m_event(cmd_id)
            , m_size(sizeof (Packet))
        {}
    };

    CommandMessage();
    virtual ~CommandMessage();
    virtual ssize_t FindHeader(uint8_t* buffer, ssize_t buffer_tail);
    virtual EncodedImage Encode(const uint8_t* rgb888, int width, int height, float quality_factor);
    virtual QImage &Decode(const EncodedImage &enc, QImage &out_image) override;
    virtual bool IsValid(uint8_t* buffer, ssize_t buffer_tail) override;
    virtual ssize_t HeaderSize() const override;
};
Q_DECLARE_METATYPE(CommandMessage::Packet);
