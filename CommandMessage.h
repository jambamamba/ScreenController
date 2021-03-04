#pragma once

#include "ImageConverterInterface.h"

struct CommandMessage : public ImageConverterInterface
{
    CommandMessage();
    virtual ~CommandMessage();
    virtual ssize_t FindHeader(uint8_t* buffer, ssize_t buffer_tail) override;
    virtual EncodedImage Encode(const uint8_t* rgb888, int width, int height, float quality_factor) override;
    virtual QImage &Decode(const EncodedImage &enc, QImage &out_image) override;
    virtual bool IsValid(uint8_t* buffer, ssize_t buffer_tail) override;
    virtual ssize_t HeaderSize() const override;
};
