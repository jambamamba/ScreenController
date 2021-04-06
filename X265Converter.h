#pragma once

#include <atomic>
#include <future>

#include "Command.h"
#include "ImageConverterInterface.h"
#include "libde265/dec265/hevcreader.h"
#include "libde265/dec265/yuvwriter.h"

class QImage;
struct X265Encoder
{
    X265Encoder(int width,
            int height,
            std::function<int(char **data, ssize_t *bytes, int width, int height, float quality_factor)> encoderInputFn,
            std::function<void(EncodedImage enc)> encoderOutputFn
    );
    virtual ~X265Encoder();
protected:
    void StartEncoderThread(int width = 0,
                            int height = 0,
                            std::function<int(char **data, ssize_t *bytes, int width, int height, float quality_factor)> encoderInputFn = nullptr,
                            std::function<void(EncodedImage enc)> encoderOutputFn = nullptr
                    );
    std::future<void> _encoder_thread;
    std::function<int(char **data, ssize_t *bytes, int width, int height, float quality_factor)> _encoderInputFn = nullptr;
    std::function<void(EncodedImage enc)> _encoderOutputFn = nullptr;
    std::atomic<bool> _die = false;
};

struct X265Decoder
{
    X265Decoder(ssize_t img_width, ssize_t img_height, std::function<void(const QImage &img)> onDecode);
    virtual ~X265Decoder();
    void Decode(uint32_t ip, ssize_t width, ssize_t height, const EncodedImage &enc);
protected:
    std::future<void> StartDecoderThread();
    HevcReader _hevc;
    YuvWriter _yuv;
    std::future<void> _decoder_thread;
    EncodedImage _img_to_decode;
    std::atomic<bool> _die = false;
};
