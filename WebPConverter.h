#pragma once

#include "ImageConverterInterface.h"

struct WebPConverter : public ImageConverterInterface
{
    WebPConverter();
    virtual ~WebPConverter();
    virtual ssize_t FindHeader(uint8_t* buffer, ssize_t buffer_tail) override;
    virtual EncodedChunk Encode(const uint8_t* rgb888, ssize_t width, ssize_t height, float quality_factor) override;
    virtual QImage Decode(const EncodedChunk &enc) override;
    virtual bool IsValid(uint8_t* buffer, ssize_t buffer_tail) override;
    virtual ssize_t HeaderSize() const override;
};

