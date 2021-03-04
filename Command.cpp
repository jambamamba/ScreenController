#include "Command.h"

Command::Command(uint8_t version, uint16_t cmd_id)
    : m_version(version)
    , m_event(cmd_id)
    , m_size(sizeof (Command))
{}
