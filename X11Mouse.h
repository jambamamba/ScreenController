#pragma once

#include "MouseInterface.h"

struct _XDisplay;
class X11Mouse : public MouseInterface
{
public:
    X11Mouse(QObject *parent);
    virtual ~X11Mouse();

    virtual QImage getMouseCursor(QPoint &pos) const override;
    virtual void mouseClick(int button) override;
    virtual void moveTo(int x, int y) override;
protected:
    _XDisplay* m_display;
};
