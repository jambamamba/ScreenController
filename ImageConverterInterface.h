#pragma once

#include <QImage>
#include <functional>

struct EncodedImage
{
    EncodedImage(uint8_t* enc_data,
                 size_t enc_sz,
                 ssize_t width,
                 ssize_t height,
                 std::function<void(uint8_t* )> free_fn)
        : m_enc_data(enc_data)
        , m_enc_sz(enc_sz)
        , m_width(width)
        , m_height(height)
        , m_free_fn(free_fn)
    {}
    EncodedImage(uint8_t* enc_data = nullptr,
                 size_t enc_sz = 0,
                 ssize_t width = 0,
                 ssize_t height = 0)
        : EncodedImage(enc_data, enc_sz, width, height, nullptr)
    {}
    EncodedImage(ssize_t width, ssize_t height, std::function<void(uint8_t* )> free_fn)
        : EncodedImage(nullptr, 0, width, height, free_fn)
    {}
    ~EncodedImage()
    {
        if(m_free_fn)
        {
            m_free_fn(m_enc_data);
        }
    }

    size_t m_enc_sz = 0;
    uint8_t* m_enc_data = nullptr;
    ssize_t m_width = 0;
    ssize_t m_height = 0;
    std::function<void(uint8_t* )> m_free_fn;
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
    virtual EncodedImage Encode(const uint8_t* rgb888, ssize_t width, ssize_t height, float quality_factor) = 0;
    virtual QImage Decode(const EncodedImage &enc) = 0;
    virtual bool IsValid(uint8_t* buffer, ssize_t buffer_tail) = 0;
    virtual ssize_t HeaderSize() const = 0;
};
