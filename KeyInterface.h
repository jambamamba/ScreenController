#pragma once

#include <QImage>
#include <QMap>
#include <QPair>
#include <functional>

struct HotKey
{
    enum Action
    {
        None,
        Record,
        ScreenShot,
    };
    Action m_action;
    quint32 m_modifiers;
    quint32 m_key;
    QString m_name;

    HotKey(Action action, quint32 modifiers, quint32 key, const QString &name)
        : m_action(action)
        , m_modifiers(modifiers)
        , m_key(key)
        , m_name(name)
    {}
    HotKey()
        : m_action(None)
        , m_modifiers(-1)
        , m_key(-1)
    {}
};

class KeyInterface : public QObject
{
    Q_OBJECT
public:
    KeyInterface(QObject *parent);
    virtual ~KeyInterface();
    virtual void keyEvent(uint32_t keyCode, uint32_t keyModifiers, uint32_t type) = 0;
    virtual bool testKeyEvent(int window, uint32_t &key, uint32_t &modifier, uint32_t &type) = 0;
    virtual bool testKey(char symbol, uint32_t key) = 0;
    virtual bool testKeyPress(uint32_t type) = 0;
    virtual bool testKeyRelease(uint32_t type) = 0;
    virtual bool testAltModifier(uint32_t modifier) = 0;
    virtual bool testControlModifier(uint32_t modifier) = 0;
    virtual bool testShiftModifier(uint32_t modifier) = 0;

protected slots:
    virtual void testHotKeyPress() = 0;
    virtual void onRegisterHotKey(quint32 key, quint32 modifiers) = 0;
    virtual void onUnRegisterHotKey(quint32 key, quint32 modifiers) = 0;
signals:
    void registerHotKey(quint32 key, quint32 modifiers) const;
    void registeredHotKey(bool success, quint32 key, quint32 modifiers) const;
    void unRegisterHotKey(quint32 key, quint32 modifiers) const;
    void hotKeyPressed(quint32 key, quint32 modifiers) const;

protected:
};
