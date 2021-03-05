#pragma once

#include <QWidget>
#include <mutex>

#include "Command.h"

namespace Ui {
class TransparentMaximizedWindow;
}

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
    void SetImage(const QImage&img);
    bool IsClosed() const;
    void ReOpen();
    
    virtual void 	keyPressEvent(QKeyEvent *event) override;
    virtual void 	keyReleaseEvent(QKeyEvent *event) override;
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;

signals:
    void Close();
    void SendCommandToNode(const Command &pkt);

private:
    Ui::TransparentMaximizedWindow *ui;
    QString m_ip;
    bool m_capturing;
    QScreen *m_screen;
    std::mutex m_mutex;
    QImage m_image;
    QTimer *m_timer;
    enum DebounceEvents : int {
        MousePress,
        MouseRelease,
        MouseMove,
        KeyPress,
        KeyRelease,
        TotalEvents
    };
    std::chrono::steady_clock::time_point m_debounce_interval[TotalEvents];
    bool m_closed = false;

    void paintEvent(QPaintEvent *);
    bool Debounce(DebounceEvents event);
//    bool eventFilter(QObject *obj, QEvent *event);
};
