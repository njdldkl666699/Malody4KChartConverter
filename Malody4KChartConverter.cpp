#include "Malody4KChartConverter.h"

#include <QFileDialog>
#include <QFile>
#include <QFileInfo>

#include <QJsonDocument>
#include <QJsonObject>

#include <QMediaPlayer>
#include <QMediaMetaData>

using ConvertStatus = Malody4KChartConverter::ConvertStatus;

Malody4KChartConverter::Malody4KChartConverter(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	connect(ui.pushButton_src, &QPushButton::clicked, this, &Malody4KChartConverter::selectSrcFile);
	connect(ui.pushButton_dst, &QPushButton::clicked, this, &Malody4KChartConverter::selectDstDir);
	connect(ui.pushButton_convert, &QPushButton::clicked, this, &Malody4KChartConverter::convert);
}

void Malody4KChartConverter::selectSrcFile()
{
	//Select Malody 4K Charts
	QFileDialog srcFileDialog(this, "选择Malody 4K铺面", ".", "Malody Chart(*.mc)");
	srcFileDialog.setFileMode(QFileDialog::ExistingFiles);
	if (srcFileDialog.exec() == QFileDialog::Accepted)
	{
		srcFilePathList = srcFileDialog.selectedFiles();
		//Set srcPathAll label text
		if (!srcFilePathList.isEmpty())
		{
			QString srcPathAll;
			srcDirPath = QFileInfo(srcFilePathList.first()).dir().path();
			for (const auto& srcFilePath : srcFilePathList)
			{
				QFileInfo srcFileInfo(srcFilePath);
				srcPathAll += srcFileInfo.fileName();
				srcPathAll += ", ";
			}
			ui.lineEdit_src->setText(srcPathAll);
		}
	}
}

void Malody4KChartConverter::selectDstDir()
{
	QFileDialog dstFileDialog(this, "选择目标文件夹", ".");
	dstFileDialog.setFileMode(QFileDialog::Directory);
	if (dstFileDialog.exec() == QFileDialog::Accepted)
	{
		dstDirPath = dstFileDialog.selectedFiles().first();
		ui.lineEdit_dst->setText(dstDirPath);
	}
}

void Malody4KChartConverter::convert()
{
	/*Note:
	create a new directory in dstDir
	is implemented in convertSingle()
	*/
	QString convertRlt;
	// Convert each chart
	for (const auto& srcFilePath : srcFilePathList)
	{
		if (convertSingle(srcFilePath) == ConvertFailed)
		{
			qDebug() << "Error: Convert failed. File: " << srcFilePath << "\n";
			convertRlt += "Convert failed：" + srcFilePath + "\n";
		}
	}
	convertRlt += "Convert finished";
	ui.label_convertRlt->setText(convertRlt);
}

ConvertStatus Malody4KChartConverter::convertSingle(const QString& srcFilePath)
{
	//Open srcFile
	QFile srcFile(srcFilePath);
	if (!srcFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		qDebug() << "Error: Open file failed. File: " << srcFilePath << "\n";
		return ConvertFailed;
	}
	QByteArray srcContent = srcFile.readAll();
	srcFile.close();

	//Parse srcContent to srcJsonObj
	QJsonDocument srcJsonDoc = QJsonDocument::fromJson(srcContent);
	if (!srcJsonDoc.isObject())
	{
		qDebug() << "Error: Parse json to object failed. File: " << srcFilePath << "\n";
		return ConvertFailed;
	}
	QJsonObject srcJsonObj = srcJsonDoc.object();

	//Parse srcJsonObj
	QJsonObject metaObj = srcJsonObj["meta"].toObject();

	//Create a new QJsonObj to store converted data
	QJsonObject dstJsonObj;

	//#### Useless ####
	//// 1. get music total time(ms)
	//QMediaPlayer musicPlayer;
	//musicPlayer.setSource(QUrl::fromLocalFile(musicFilePath));
	////int musicTotalTime = musicPlayer.metaData()["Duration"].toInt();
	//musicDuration = musicPlayer.metaData()[QMediaMetaData::Duration].toInt();

	// 2. get offset
	QJsonArray noteArray = srcJsonObj["note"].toArray();
	QJsonObject miscObj = noteArray.last().toObject();
	int chartOffset = miscObj["offset"].toInt();

	/* 3. get note array and write to dstJsonObj
	Example:
	"note": [
		{
			"time": 0,
			"column": 0
		},
		{
			"time": 1000,
			"endTime": 2000,
			"column": 3
		}
	]
	*/
	QJsonArray bpmArray = srcJsonObj["time"].toArray();
	// 3.1 calculate the "beatNum" of each bpm
	QHash<double, double> bpmHash;
	for (const auto& bpmVal : bpmArray)
	{
		const auto& bpmObj = bpmVal.toObject();
		const auto& beatArr = bpmObj["beat"].toArray();
		double beatNum = getBeatNum(beatArr);
		bpmHash[beatNum] = bpmObj["bpm"].toDouble();
	}

	// 3.2 calculate the time(and endTime) of each note
	QJsonArray dstNoteArray;
	for (const auto& noteVal : noteArray)
	{
		const auto& noteObj = noteVal.toObject();
		const auto& beatArr = noteObj["beat"].toArray();
		double beatNum = getBeatNum(beatArr);
		/*
		handle with the last data, it is not a note,
		and its sign is beatNum == 0
		*/
		if (beatNum == 0.0)
		{
			continue;
		}

		//find which range the beatNum belongs to
		double bpmKey = 0;
		for (const auto& key : bpmHash.keys())
		{
			/* Example:
			bpmHash.keys() = {0,100,200,300}
			beatNum = 150
			so bpm should be bpmHash[100]
			*/
			if (key > beatNum)
			{
				break;
			}
			bpmKey = key;
		}
		double bpm = bpmHash[bpmKey];
		int noteTime = getNoteTime(beatArr, bpm, chartOffset);

		QJsonObject dstNoteObj;
		dstNoteObj["column"] = noteObj["column"].toInt();
		dstNoteObj["time"] = noteTime;

		//deal with endTime
		if (noteObj.contains("endbeat"))
		{
			const auto& endBeatArr = noteObj["endbeat"].toArray();
			//notice that the end bpm may be different from the start bpm
			double endBpmKey = 0;
			for (const auto& key : bpmHash.keys())
			{
				if (key > beatNum)
				{
					break;
				}
				endBpmKey = key;
			}
			double endBpm = bpmHash[endBpmKey];
			int endNoteTime = getNoteTime(endBeatArr, endBpm, chartOffset);
			dstNoteObj["endTime"] = endNoteTime;
		}
		dstNoteArray.append(dstNoteObj);
	}

	// 3.3 write to dstJsonObj
	dstJsonObj["note"] = dstNoteArray;

	//Create a new directory in dstDir
	dstDir.cd(dstDirPath);
	QJsonObject songObj = metaObj["song"].toObject();
	QString dstDirName = songObj["title"].toString();
	dstDir.mkdir(dstDirName);
	dstDir.cd(dstDirName);
	//now dstDir is exactly in the new directory

	//copy music
	QString musicName = miscObj["sound"].toString();
	QString musicSrcPath = srcDirPath + "/" + musicName;
	QString musicDstPath = dstDir.path() + "/" + musicName;
	if (!QFile::copy(musicSrcPath, musicDstPath))
	{
		qDebug() << "Warning: Copy music failed. sound: " << musicName << "\n";
	}
	//copy background
	QString bgName = metaObj["background"].toString();
	QString bgSrcPath = srcDirPath + "/" + bgName;
	QString bgDstPath = dstDir.path() + "/" + bgName;
	if (!QFile::copy(bgSrcPath, bgDstPath))
	{
		qDebug() << "Warning: Copy background failed. background:" << bgName << "\n";
	}

	//Create a new file in dstDir and write dstJsonObj to it
	QString dstFileName = metaObj["version"].toString();
	//A Json File isn't always ".json", Meolide use .meo   ;D  😽[猫脸亲亲]
	QFile dstFile(dstDir.path() + "/" + dstFileName + ".meo");
	dstFile.open(QIODevice::Truncate | QIODevice::WriteOnly | QIODevice::Text);
	if (!dstFile.isOpen())
	{
		qDebug() << "Error: Open file failed. File: " << dstFileName << "\n";
		return ConvertFailed;
	}
	QJsonDocument dstJsonDoc(dstJsonObj);
	dstFile.write(dstJsonDoc.toJson());
	dstFile.close();
	return ConvertSuccess;
}