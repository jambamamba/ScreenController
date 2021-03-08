#pragma once

#include <QImage>
#include <QMap>
#include <QPair>

class MouseInterface : public QObject
{
    Q_OBJECT
public:
    MouseInterface(QObject *parent);
    virtual ~MouseInterface();

    virtual QImage getMouseCursor(QPoint &pos) const = 0;
    virtual void buttonPress(int button, int x, int y) = 0;
    virtual void buttonRelease(int button, int x, int y) = 0;
    virtual void moveTo(int x, int y) = 0;
};
