#include "MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QStandardPaths>

#include "DiscoveryService.h"
#include "DiscoveryClient.h"

extern "C" void mylog(const char *fmt, ...)
{
    char tbuffer [128];
    time_t t = time(0);
    tm * timeinfo = localtime ( &t );
    strftime (tbuffer, 80,"%y-%m-%d %H:%M:%S", timeinfo);

    char buffer1[1024];
    char buffer2[1024+128];
    va_list args;
    va_start(args, fmt);
    vsprintf (buffer1, fmt, args);
    sprintf(buffer2, "%s - %s\n", tbuffer, buffer1);
    QString *msg = new QString(buffer2);
    qDebug() << *msg;

    static QString output_dir(QStandardPaths::writableLocation(
                                  QStandardPaths::DocumentsLocation) + "/screen-recorder.log");
    FILE* fp = fopen(output_dir.toUtf8().data(), "a+t");
    if(fp) {
        fwrite(buffer2, strlen(buffer2), 1, fp);
        fclose(fp);
    }

    va_end(args);
}

int main(int argc, char *argv[])
{
    bool stop = false;
    auto discovery_thread = std::async(std::launch::async, [&stop](){
        DiscoveryService discovery;
        DiscoveryClient client;
        while(!stop)
        {
            client.Discover();
            usleep(3 * 1000 * 1000);
        }
    });

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    int ret = a.exec();
    stop = true;
    discovery_thread.wait();
    return ret;
}
