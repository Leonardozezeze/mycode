#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->greenButon,&QPushButton::clicked,this,[this](){
        const QString name=ui->nameEdit->text().trimmed();
        if(name.isEmpty())
            ui->result_Label->setText("请先输入名字!");
        else
            ui->result_Label->setText("你好"+name+"!");
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
