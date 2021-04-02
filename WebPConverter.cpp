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
    for(; idx < buffer_tail - HeaderSize(); ++idx)
    {
//webp header                52 49 46 46 08 32 01 00 57 45 42 50 56 50 38
        if(buffer[idx] == 'R' &&
                buffer[idx+1] == 'I' &&
                buffer[idx+2] == 'F' &&
                buffer[idx+3] == 'F' &&
//                buffer[idx+5] == 0x32 &&
//                buffer[idx+6] == 0x01 &&
//                buffer[idx+7] == 0x00 &&
                buffer[idx+8] == 'W' &&
                buffer[idx+9] == 'E' &&
                buffer[idx+10] == 'B' &&
                buffer[idx+11] == 'P'
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
    EncodedImage enc([](uint8_t *data){
        if(data) { WebPFree(data); }
    });
    enc.m_enc_sz = WebPEncodeRGB(rgb888, width, height, width * 3, quality_factor, &enc.m_enc_data);
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
    size_t data_size = width * height * 3;
    uint8_t* rgb888 = WebPDecodeRGBInto(enc.m_enc_data, enc.m_enc_sz,
                                        out_image.bits(),
                                        data_size,
                                        width * 3);
    return out_image;
}

bool WebPConverter::IsValid(uint8_t *buffer, ssize_t buffer_tail)
{
    int width = 0;
    int height = 0;
    return WebPGetInfo(buffer, buffer_tail, &width, &height);
}

ssize_t WebPConverter::HeaderSize() const
{
    return 13;
}
