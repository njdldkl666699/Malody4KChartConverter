#include "Malody4KChartConverter.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Malody4KChartConverter w;
    w.show();
    return a.exec();
}
