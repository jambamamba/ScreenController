#pragma once

#include "ImageConverterInterface.h"

struct CommandMessage : public ImageConverterInterface
{
    struct Packet
    {
        uint8_t m_sync_bytes[4] = { 0xca, 0xfe, 0xba, 0xbe };
        uint8_t m_version;
        uint8_t m_padding;
        uint16_t m_cmd_id;
        uint32_t m_size;
        uint8_t m_tail_bytes[4] = { 0xca, 0xfe, 0xd0, 0x0d };
    };

    CommandMessage();
    virtual ~CommandMessage();
    virtual ssize_t FindHeader(uint8_t* buffer, ssize_t buffer_tail);
    virtual EncodedImage Encode(const uint8_t* rgb888, int width, int height, float quality_factor);
    virtual QImage &Decode(const EncodedImage &enc, QImage &out_image) override;
    virtual bool IsValid(uint8_t* buffer, ssize_t buffer_tail) override;
    virtual ssize_t HeaderSize() const override;
};

