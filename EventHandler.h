#pragma once

#include <QObject>

struct Command;
class KeyInterface;
class MouseInterface;
class EventHandler : public QObject
{
    Q_OBJECT
public:
    EventHandler(QObject *parent);
    void HandleCommand(const Command &pkt, uint32_t ip);

signals:
    void StartStreaming(uint32_t ip, int decoder_type);
    void StopStreaming(uint32_t ip);
    void StoppedStreaming(uint32_t ip);
protected:
    MouseInterface *m_mouse;
    KeyInterface *m_key;
};
