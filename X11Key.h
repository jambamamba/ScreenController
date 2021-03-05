#pragma once

#include "KeyInterface.h"

struct _XDisplay;
class X11Key : public KeyInterface
{
public:
    X11Key(QObject *parent);
    virtual ~X11Key();
    virtual void keyPress(unsigned int keyCode, unsigned int keyModifiers) override;
    virtual void keyRelease(unsigned int keyCode, unsigned int keyModifiers) override;
    virtual bool testKeyEvent(int window, uint32_t &key, uint32_t &modifier, int &type) override;

protected slots:
    virtual void testHotKeyPress() override;
    virtual void onRegisterHotKey(quint32 key, quint32 modifiers) override;
    virtual void onUnRegisterHotKey(quint32 key, quint32 modifiers) override;

protected:
    _XDisplay* m_display;
    int m_registered_hotkey_count;
};
