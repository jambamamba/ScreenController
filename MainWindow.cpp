#include "MainWindow.h"

#include <QDebug>
#include <QListWidgetItem>
#include <QStandardItemModel>

#include "ui_MainWindow.h"

#include "DiscoveryService.h"
#include "DiscoveryClient.h"
#include "TransparentMaximizedWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_transparent_window(nullptr)
    , m_streamer(m_streamer_socket)
{
    ui->setupUi(this);
    m_node_model = new QStandardItemModel(ui->listView);
    connect(ui->listView, &QListView::doubleClicked,this,&MainWindow::NodeDoubleClicked);

    ui->listView->setModel(m_node_model);
    ui->listView->show();

    StartDiscoveryService();
    PrepareToReceiveStream();
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
        DiscoveryClient client([this](DiscoveryData *data, std::string ip, uint16_t port){
            qDebug() << "Discovered node " << QString(data->m_name) << " at " << QString(ip.c_str()) << ":" << port;
            if(m_nodes.find(ip.c_str()) == m_nodes.end())
            {
                m_nodes.insert(ip.c_str(), new Node(data->m_name, ip, port));
                m_node_model->appendRow(new QStandardItem(QIcon(":/resources/laptop-icon-19517.png"), data->m_name));
            }
        });
        while(!m_stop)
        {
            client.Discover();
            usleep(3 * 1000 * 1000);
        }
    });

}

void MainWindow::NodeDoubleClicked(QModelIndex index)
{
    showTransparentWindowOverlay();

    int idx = 0;
    for(const auto &node : m_nodes)
    {
        if(idx == index.row())
        {
            m_streamer.StartStreaming(node->m_ip, node->m_port);
            break;
        }
        idx ++;
    }
}

void MainWindow::PrepareToReceiveStream()
{
    m_streamer_socket.PlaybackImages([this](const QImage&img){
        qDebug() << "############### got image";
        if(!m_transparent_window)
        {
            showTransparentWindowOverlay();
        }
        m_transparent_window->SetImage(img);
        m_transparent_window->hide();
        m_transparent_window->showFullScreen();
    });
    m_streamer_socket.StartRecieveDataThread();
}

void MainWindow::showTransparentWindowOverlay()
{
    QImage screen_shot = m_streamer.ScreenShot();

    m_transparent_window = new TransparentMaximizedWindow(this);
    m_transparent_window->show(screen_shot.width(), screen_shot.height(), m_streamer.ActiveScreen());
}

void MainWindow::on_testButton_clicked()
{
    showTransparentWindowOverlay();
}
