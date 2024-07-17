#pragma once

#include <QtWidgets/QWidget>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include "ui_Malody4KChartConverter.h"

class Malody4KChartConverter : public QWidget
{
    Q_OBJECT

public:
    enum ConvertStatus
	{
		ConvertSuccess = 0,
		ConvertFailed = 1
	};

public:
    Malody4KChartConverter(QWidget *parent = nullptr);
    ~Malody4KChartConverter();

    void selectSrcFile();
    void selectDstDir();
    void convert();
    ConvertStatus convertSingle(const QString& srcFilePath) const;

private:
    Ui::Malody4KChartConverterClass ui;

    QStringList srcFilePathList;
    QString dstDirPath;
};
