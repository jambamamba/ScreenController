#pragma once

#include "ImageConverterInterface.h"

struct WebPConverter : public ImageConverterInterface
{
    WebPConverter();
    virtual ~WebPConverter();
    virtual ssize_t FindHeader(uint8_t* buffer, ssize_t buffer_tail);
    virtual EncodedImage Encode(const uint8_t* rgb888, int width, int height, float quality_factor);
    virtual QImage &Decode(const EncodedImage &enc, QImage &out_image) override;
    virtual bool IsValid(uint8_t* buffer, ssize_t buffer_tail) override;
};

