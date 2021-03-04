#pragma once

#include <QObject>

struct Command;
class MouseInterface;
class EventHandler : public QObject
{
    Q_OBJECT
public:
    EventHandler(QObject *parent);
    void HandleCommand(const Command &pkt, uint32_t ip);

signals:
    void StartStreaming(uint32_t ip, int decoder_type);
protected:
    MouseInterface *m_mouse;
};
