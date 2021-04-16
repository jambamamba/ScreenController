#include "Command.h"

Command::Command(uint16_t event)
    : m_event(event)
    , m_size(sizeof (Command))
{
}
