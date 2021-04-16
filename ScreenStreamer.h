#pragma once

#include <future>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <QList>
#include <QImage>
#include <QTimer>

#include "Command.h"

class MouseInterface;
class QScreen;
class ScreenStreamer : public QObject
{
    Q_OBJECT

public:
    ScreenStreamer(std::function<void(uint32_t ip, const Command &cmd)> sendCommand, QObject *parent);
    ~ScreenStreamer();
    QImage ScreenShot();
    QScreen *ActiveScreen();

public slots:
    void StartStreaming(uint32_t ip, uint32_t sequence_number, uint32_t decoder_type);
    void StopStreaming(uint32_t ip);

protected:
    void InitAvailableScreens();
    int ActiveScreenIdx() const;
    QImage& ApplyMouseCursor(QImage& img);

    QList<QScreen *> _screens;
    MouseInterface *m_mouse;
    bool _streaming = false;
    std::function<void(uint32_t ip, const Command &cmd)> _sendCommand = nullptr;
    std::atomic<bool> _die;
    void StopThreads();
};
