#pragma once

#include <QImage>

namespace JpegConverter
{
    ssize_t FindJpegHeader(uint8_t* buffer, ssize_t buffer_tail);
    bool ValidJpegFooter(uint8_t second_last_byte, uint8_t last_byte);
    QImage FromJpeg(uint8_t *jpg_buffer, size_t jpg_size);
    void ToJpeg(unsigned char* image, int width, int height, int quality,
                const char* comment, unsigned long* jpegSize, unsigned char** jpegBuf);
}
