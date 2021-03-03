#include "WebPConverter.h" //https://developers.google.com/speed/webp/docs/api

#include <QDebug>
#include <webp/encode.h>
#include <webp/decode.h>

WebPConverter::WebPConverter()
{}

WebPConverter::~WebPConverter()
{}

ssize_t WebPConverter::FindHeader(uint8_t *buffer, ssize_t buffer_tail)
{
    ssize_t idx = 0;
    for(; idx < buffer_tail - 15; ++idx)
    {
//webp header                52 49 46 46 08 32 01 00 57 45 42 50 56 50 38
        if(buffer[idx] == 0x52 &&
                buffer[idx+1] == 0x49 &&
                buffer[idx+2] == 0x46 &&
                buffer[idx+3] == 0x46 &&
                buffer[idx+4] == 0x08 &&
                buffer[idx+5] == 0x32 &&
                buffer[idx+6] == 0x01 &&
                buffer[idx+7] == 0x00 &&
                buffer[idx+8] == 0x57 &&
                buffer[idx+9] == 0x45 &&
                buffer[idx+10] == 0x42 &&
                buffer[idx+11] == 0x50 &&
                buffer[idx+12] == 0x56 &&
                buffer[idx+13] == 0x50 &&
                buffer[idx+14] == 0x38 &&
                buffer[idx+15] == 0x20
                )
        {
            return idx;
        }
    }
    return -1;
}

EncodedImage WebPConverter::Encode(const uint8_t* rgb888,
                                   int width,
                                   int height,
                                   float quality_factor)
{
    EncodedImage enc;
    enc.m_enc_sz = (quality_factor > 100) ?
                WebPEncodeLosslessRGB(rgb888, width, height, width * 3, &enc.m_enc_data):
                WebPEncodeRGB(rgb888, width, height, width * 3, quality_factor, &enc.m_enc_data);
    return enc;
}

QImage &WebPConverter::Decode(const EncodedImage &enc, QImage &out_image)
{
    int width = 0;
    int height = 0;
    if(!WebPGetInfo(enc.m_enc_data, enc.m_enc_sz, &width, &height))
    {
        return out_image;
    }
    uint8_t* rgb888 = WebPDecodeRGB(enc.m_enc_data, enc.m_enc_sz, &width, &height);

    if(out_image.isNull() || out_image.width() != width || out_image.height() != height)
    {
        out_image = QImage(width, height, QImage::Format::Format_RGB888);
    }
    size_t data_size = width * height * 3;
    memcpy(out_image.bits(), rgb888, data_size);

    WebPFree(rgb888);
    return out_image;
}

bool WebPConverter::IsValid(uint8_t *buffer, ssize_t buffer_tail)
{
    int width = 0;
    int height = 0;
    return WebPGetInfo(buffer, buffer_tail, &width, &height);
}
