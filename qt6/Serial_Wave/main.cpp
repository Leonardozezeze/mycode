#include "mainwindow.h"
#include "apptheme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    applyAppTheme(a);

    MainWindow w;
    w.show();
    return QApplication::exec();
}
