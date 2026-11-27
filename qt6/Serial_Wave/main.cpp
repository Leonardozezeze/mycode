#include "mainwindow.h"
#include "apptheme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    // Keep the storage identity for existing settings and recorded sessions.
    QCoreApplication::setOrganizationName(QStringLiteral("SerialWave"));
    QCoreApplication::setApplicationName(QStringLiteral("Serial_Wave"));
    QApplication::setApplicationDisplayName(QStringLiteral("迹波"));
    applyAppTheme(a);

    MainWindow w;
    w.show();
    return QApplication::exec();
}
