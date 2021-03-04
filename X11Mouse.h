#pragma once

#include "MouseInterface.h"

struct _XDisplay;
class X11Mouse : public MouseInterface
{
public:
    X11Mouse(QObject *parent);
    virtual ~X11Mouse();

    virtual QImage getMouseCursor(QPoint &pos) const override;
    virtual void mousePress(int button) override;
    virtual void mouseRelease(int button) override;
    virtual void moveTo(int x, int y) override;

protected:
    void mouseClick(int button, int press_or_release);
    _XDisplay* m_display;
};
