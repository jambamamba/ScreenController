#pragma once

#include <QImage>
#include <functional>

struct EncodedChunk
{
    EncodedChunk(uint8_t* enc_data,
                 size_t enc_sz,
                 ssize_t width,
                 ssize_t height,
                 std::function<void(uint8_t* )> free_fn)
        : _data(enc_data)
        , _size(enc_sz)
        , _width(width)
        , _height(height)
        , _freeFn(free_fn)
    {}
    EncodedChunk(uint8_t* enc_data = nullptr,
                 size_t enc_sz = 0,
                 ssize_t width = 0,
                 ssize_t height = 0)
        : EncodedChunk(enc_data, enc_sz, width, height, nullptr)
    {}
    EncodedChunk(ssize_t width, ssize_t height, std::function<void(uint8_t* )> free_fn)
        : EncodedChunk(nullptr, 0, width, height, free_fn)
    {}
    ~EncodedChunk()
    {
        if(_freeFn)
        {
            _freeFn(_data);
        }
    }

    size_t _size = 0;
    uint8_t* _data = nullptr;
    ssize_t _width = 0;
    ssize_t _height = 0;
    std::function<void(uint8_t* )> _freeFn = nullptr;
};

struct ImageConverterInterface
{
    enum Types {
        None,
        Command,
        Jpeg,
        Webp,
        X265
    };

    ImageConverterInterface() {}
    virtual ~ImageConverterInterface() {}
    virtual ssize_t FindHeader(uint8_t* buffer, ssize_t buffer_tail) = 0;
    virtual EncodedChunk Encode(const uint8_t* rgb888, ssize_t width, ssize_t height, float quality_factor) = 0;
    virtual QImage Decode(const EncodedChunk &enc) = 0;
    virtual bool IsValid(uint8_t* buffer, ssize_t buffer_tail) = 0;
    virtual ssize_t HeaderSize() const = 0;
};
