#pragma once

#include <QtWidgets/QWidget>
#include "ui_Malody4KChartConverter.h"

class Malody4KChartConverter : public QWidget
{
    Q_OBJECT

public:
    Malody4KChartConverter(QWidget *parent = nullptr);
    ~Malody4KChartConverter();

private:
    Ui::Malody4KChartConverterClass ui;
};
