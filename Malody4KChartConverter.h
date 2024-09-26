#pragma once

#include "ui_Malody4KChartConverter.h"
#include <QtWidgets/QWidget>
#include <QDir>
#include <QJsonArray>

class Malody4KChartConverter : public QWidget
{
    Q_OBJECT

public:
    enum ConvertStatus
	{
		ConvertSuccess,
		ConvertFailed
	};

public:
    Malody4KChartConverter(QWidget *parent = nullptr);
    ~Malody4KChartConverter() {};

private:
    struct bpmInfo
    {
        double beatNum;
        double bpm;
        double time;
    };

private:
    void selectSrcFile();
    void selectDstDir();
    void convert();
    ConvertStatus convertSingle(const QString& srcFilePath);

    double getBeatNum(const QJsonArray& beatArr)const
    {
        int beat = beatArr.at(0).toInt();
        int pos = beatArr.at(1).toInt();
        int totalPos = beatArr.at(2).toInt();
        return beat + double(pos) / totalPos;
    }

private:
    Ui::Malody4KChartConverterClass ui;

    QStringList srcFilePathList;
    QString srcDirPath;
    QString dstDirPath;
	QDir dstDir;
};
