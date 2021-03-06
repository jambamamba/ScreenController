#pragma once

#include <QObject>
#include <memory>

struct Command;
class KeyInterface;
class MouseInterface;
class EventHandler : public QObject
{
    Q_OBJECT
public:
    EventHandler(QObject *parent);
    ~EventHandler();
    void HandleCommand(const Command &pkt, uint32_t ip);

signals:
    void StartStreaming(uint32_t ip, int decoder_type);
    void StopStreaming(uint32_t ip);
    void StoppedStreaming(uint32_t ip);
protected:
    std::unique_ptr<MouseInterface> m_mouse;
    std::unique_ptr<KeyInterface> m_key;
};
