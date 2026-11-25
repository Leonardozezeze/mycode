#include "mainwindow.h"
#include "apptheme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SerialWave"));
    QCoreApplication::setApplicationName(QStringLiteral("Serial_Wave"));
    applyAppTheme(a);

    MainWindow w;
    w.show();
    return QApplication::exec();
}
