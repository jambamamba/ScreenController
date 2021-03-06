#pragma once

#include "KeyInterface.h"

struct _XDisplay;
class X11Key : public KeyInterface
{
public:
    X11Key(QObject *parent);
    virtual ~X11Key();
    virtual void keyEvent(uint32_t key, uint32_t modifier, uint32_t type) override;
    virtual bool testKeyEvent(int window, uint32_t &key, uint32_t &modifier, uint32_t &type) override;
    virtual bool testKey(char symbol, uint32_t key) override;
    virtual bool testKeyPress(uint32_t type) override;
    virtual bool testKeyRelease(uint32_t type) override;
    virtual bool testAltModifier(uint32_t modifier) override;
    virtual bool testControlModifier(uint32_t modifier) override;
    virtual bool testShiftModifier(uint32_t modifier) override;
protected slots:
    virtual void testHotKeyPress() override;
    virtual void onRegisterHotKey(quint32 key, quint32 modifiers) override;
    virtual void onUnRegisterHotKey(quint32 key, quint32 modifiers) override;

protected:
    _XDisplay* m_display;
    int m_registered_hotkey_count;
};
