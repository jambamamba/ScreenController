#pragma once

#include <QWidget>
#include <future>
#include <mutex>
#include <memory>

#include "Command.h"
#include "Frame.h"

namespace Ui {
class TransparentMaximizedWindow;
}

class KeyInterface;
class MouseInterface;
struct Command;
class TransparentMaximizedWindow : public QWidget
{
    Q_OBJECT

public:
    explicit TransparentMaximizedWindow(const QString &ip, QWidget *parent = 0);
    ~TransparentMaximizedWindow();

    void Show(int width, int height, QScreen *screen);
    void StartCapture(const QPoint &point_start,
                      const QPoint &point_stop);
    void MoveToScreen(const QScreen *screen);
    void SetImage(const Frame &frame);
    bool IsClosed() const;
    
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;

signals:
    void Close();
    void SendCommandToNode(const Command &pkt);

private:
    Ui::TransparentMaximizedWindow *ui;
    QString m_ip;
    bool m_capturing;
    QScreen *m_screen;
    std::mutex m_mutex;
    Frame m_frame;
    QTimer *m_timer;
    QPoint m_mouse_pos;
    enum DebounceEvents : int {
        None,
        MousePress,
        MouseRelease,
        MouseDoubleClicked,
        MouseMove,
        KeyPress,
        KeyRelease,
        TotalEvents
    };
    std::chrono::steady_clock::time_point m_debounce_interval[TotalEvents];
    DebounceEvents m_mouse_button_state = DebounceEvents::None;
    std::unique_ptr<MouseInterface> m_mouse;
    std::unique_ptr<KeyInterface> m_key;
    std::future<void> m_event_capture_thread;
    bool m_stop = false;

    virtual void paintEvent(QPaintEvent *) override;
    bool Debounce(DebounceEvents event, int *out_elapsed = nullptr);
    void StartKeyCapture();
//    bool eventFilter(QObject *obj, QEvent *event);
};
