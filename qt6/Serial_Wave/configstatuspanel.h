#ifndef CONFIGSTATUSPANEL_H
#define CONFIGSTATUSPANEL_H

#include <QWidget>

class ConfigStatusPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigStatusPanel(QWidget *parent = nullptr);

signals:

private:
    QWidget* createConfigWidget();
    QWidget* createStatusWidget();
};

#endif // CONFIGSTATUSPANEL_H
