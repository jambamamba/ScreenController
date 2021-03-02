#pragma once

#include <QWidget>
#include <mutex>

namespace Ui {
class TransparentMaximizedWindow;
}

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
    
    virtual void 	keyPressEvent(QKeyEvent *event);
    virtual void 	keyReleaseEvent(QKeyEvent *event);
//    virtual void mousePressEvent(QMouseEvent *mouse_event);
//    virtual void mouseReleaseEvent(QMouseEvent *mouse_event);
//    virtual void mouseMoveEvent(QMouseEvent *mouse_event);

signals:
    void Close();
private:
    Ui::TransparentMaximizedWindow *ui;
    QString m_ip;
    bool m_capturing;
    QScreen *m_screen;
    std::mutex m_mutex;
    QImage m_image;
    QTimer *m_timer;

    void paintEvent(QPaintEvent *);
//    bool eventFilter(QObject *obj, QEvent *event);
};
