#include "CommandMessage.h"

#include <QDebug>

CommandMessage::CommandMessage()
{}
CommandMessage::~CommandMessage()
{}

ssize_t CommandMessage::FindHeader(uint8_t *buffer, ssize_t buffer_sz)
{
    ssize_t idx = 0;
    for(; idx < buffer_sz - HeaderSize(); ++idx)
    {
        if(buffer[idx] == 0xca &&
                buffer[idx+1] == 0xfe &&
                buffer[idx+2] == 0xba &&
                buffer[idx+3] == 0xbe
                )
        {
            return idx;
        }
    }
    return -1;
}

EncodedImage CommandMessage::Encode(const uint8_t *, ssize_t width, ssize_t height, float )
{
    EncodedImage enc(width, height, [](uint8_t *){
    });
    return enc;
}

QImage CommandMessage::Decode(const EncodedImage &)
{
    return QImage();
}

bool CommandMessage::IsValid(uint8_t *buffer, ssize_t sz)
{
    bool ret = (buffer[0] == 0xca && buffer[1] == 0xfe && buffer[2] == 0xba && buffer[3] == 0xbe) &&
            (buffer[sz-4] == 0xca && buffer[sz-3] == 0xfe && buffer[sz-2] == 0xd0 && buffer[sz-1] == 0x0d);
    return ret;
}

ssize_t CommandMessage::HeaderSize() const
{
    return 4;
}
