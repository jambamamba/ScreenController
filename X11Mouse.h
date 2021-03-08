#pragma once

#include "MouseInterface.h"

struct _XDisplay;
class X11Mouse : public MouseInterface
{
public:
    X11Mouse(QObject *parent);
    virtual ~X11Mouse();

    virtual QImage getMouseCursor(QPoint &pos) const override;
    virtual void buttonPress(int button, int x, int y) override;
    virtual void buttonRelease(int button, int x, int y) override;
    virtual void moveTo(int x, int y) override;

protected:
    void mouseClick(int button, int press_or_release, int x, int y);
    _XDisplay* m_display;
};
