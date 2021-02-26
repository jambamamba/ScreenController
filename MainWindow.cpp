#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "TransparentMaximizedWindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_transparent_window(nullptr)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::scrubScreenCaptureModeOperation()
{
    delete m_transparent_window;
    m_transparent_window = nullptr;
}

void MainWindow::captureFullScreen()
{
    scrubScreenCaptureModeOperation();
    m_region = QRect();
}

void MainWindow::showTransparentWindowOverlay()
{
    scrubScreenCaptureModeOperation();
    QImage screen_shot = m_streamer.ScreenShot();

    m_transparent_window = new TransparentMaximizedWindow(this);
    m_transparent_window->show(screen_shot.width(), screen_shot.height(), m_streamer.ActiveScreen());
}

void MainWindow::on_testButton_clicked()
{
    showTransparentWindowOverlay();
}
