#include "MainWindow.h"

#include <QDebug>

#include "ui_MainWindow.h"

#include "DiscoveryService.h"
#include "DiscoveryClient.h"
#include "TransparentMaximizedWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_transparent_window(nullptr)
{
    StartDiscoveryService();
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    m_stop = true;
    m_discovery_thread.wait();
    delete ui;
}

void MainWindow::StartDiscoveryService()
{
    m_discovery_thread = std::async(std::launch::async, [this](){
        DiscoveryService discovery;
        DiscoveryClient client([](DiscoveryData *data, std::string ip, uint16_t port){
            qDebug() << "Discovered node " << QString(data->m_name) << " at " << QString(ip.c_str()) << ":" << port;
        });
        while(!m_stop)
        {
            client.Discover();
            usleep(3 * 1000 * 1000);
        }
    });

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
