#include "Command.h"

Command::Command(uint16_t event,
                 uint32_t sequence_num,
                 int decoder)
    : m_event(event)
    , m_size(sizeof (Command))
{
    u.m_frame.m_sequence_number = sequence_num;
    u.m_frame.m_decoder_type = decoder;
}
