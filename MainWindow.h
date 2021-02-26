#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "ScreenStreamer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TransparentMaximizedWindow;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void captureFullScreen();
    void scrubScreenCaptureModeOperation();

private slots:
    void showTransparentWindowOverlay();

    void on_testButton_clicked();

private:
    Ui::MainWindow *ui;
    TransparentMaximizedWindow *m_transparent_window;
    QRect m_region;
    ScreenStreamer m_streamer;
};
#endif // MAINWINDOW_H
