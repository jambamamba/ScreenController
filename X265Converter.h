#pragma once

#include <atomic>
#include <future>

#include "ImageConverterInterface.h"
#include "libde265/dec265/hevcreader.h"
#include "libde265/dec265/yuvwriter.h"

class QImage;
struct X265Converter : public ImageConverterInterface
{
    X265Converter(int width,
            int height,
            std::function<int(char **data, ssize_t *bytes, int width, int height, float quality_factor)> encoderInputFn,
            std::function<void(EncodedImage enc)> encoderOutputFn
    );
    X265Converter(
            std::function<void(QImage &out_image)> decoderOutputFn
            );
    virtual ~X265Converter();
    virtual ssize_t FindHeader(uint8_t* buffer, ssize_t buffer_tail) override;
    virtual QImage &Decode(const EncodedImage &enc, QImage &out_image) override;
    virtual bool IsValid(uint8_t* buffer, ssize_t buffer_tail) override;
    virtual ssize_t HeaderSize() const override;
protected:
    void StartEncoderThread(int width = 0,
                            int height = 0,
                            std::function<int(char **data, ssize_t *bytes, int width, int height, float quality_factor)> encoderInputFn = nullptr,
                            std::function<void(EncodedImage enc)> encoderOutputFn = nullptr
                    );
    void StartDecoderThread();
    QImage *_decoded_image = nullptr;
    std::function<void(QImage &out_image)> _on_decode_cb = nullptr;
    std::future<void> _decoder_thread;
    HevcReader _hevc;
    YuvWriter _yuv;
    std::future<void> _encoder_thread;
    std::function<int(char **data, ssize_t *bytes, int width, int height, float quality_factor)> _encoderInputFn = nullptr;
    std::function<void(EncodedImage enc)> _encoderOutputFn = nullptr;
    std::atomic<bool> _die = false;
};

