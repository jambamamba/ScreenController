#pragma once

#include <QWidget>

namespace Ui {
class TransparentMaximizedWindow;
}

class TransparentMaximizedWindow : public QWidget
{
    Q_OBJECT

public:
    explicit TransparentMaximizedWindow(QWidget *parent = 0);
    ~TransparentMaximizedWindow();

    void show(int width, int height, QScreen *screen);
    void startCapture(const QPoint &point_start,
                      const QPoint &point_stop);
    void moveToScreen(const QScreen *screen);
    

private:
    Ui::TransparentMaximizedWindow *ui;
    bool m_capturing;
    QScreen *m_screen;

    virtual void mousePressEvent(QMouseEvent *mouse_event);
    virtual void mouseReleaseEvent(QMouseEvent *mouse_event);
    virtual void mouseMoveEvent(QMouseEvent *mouse_event);
};
