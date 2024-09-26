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

	/* Get note array and write to dstJsonObj
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
	// 1. Init bpmInfoVec
	QJsonArray bpmArray = srcJsonObj["time"].toArray();
	QVector<bpmInfo> bpmInfoVec;
	for (const auto& bpmVal : bpmArray)
	{
		const auto& bpmObj = bpmVal.toObject();
		const auto& beatArr = bpmObj["beat"].toArray();
		bpmInfo bpmInfo{};
		bpmInfo.beatNum = getBeatNum(beatArr);
		bpmInfo.bpm = bpmObj["bpm"].toDouble();
		bpmInfo.time = 0.0;
		bpmInfoVec.push_back(bpmInfo);
	}
	//calculate the time of each bpmInfo
	for (qsizetype i = 1; i < bpmInfoVec.size(); i++)
	{
		double deltaBeat = bpmInfoVec[i].beatNum - bpmInfoVec[i - 1].beatNum;
		double deltaTime = deltaBeat * 60000.0 / bpmInfoVec[i - 1].bpm;
		bpmInfoVec[i].time = bpmInfoVec[i - 1].time + deltaTime;
	}

	// 2. Calculate the time (and endTime) of each note
	QJsonArray noteArray = srcJsonObj["note"].toArray();
	QJsonArray dstNoteArray;
	QJsonObject miscObj;
	for (const auto& noteVal : noteArray)
	{
		const auto& noteObj = noteVal.toObject();
		const auto& beatArr = noteObj["beat"].toArray();
		double beatNum = getBeatNum(beatArr);

		//Handle with the fake note data,
		//which stores offset and music name
		if (beatNum == 0.0)
		{
			miscObj = noteObj;
			continue;
		}
		
		//find which range the beatNum belongs to
		int bpmIndex = 0;
		for (qsizetype i = 0; i < bpmInfoVec.size(); i++)
		{
			if (beatNum >= bpmInfoVec[i].beatNum)
			{
				bpmIndex = i;
			}
			else
			{
				break;
			}
		}

		//calculate the time of the note
		bpmInfo curBpmInfo = bpmInfoVec[bpmIndex];
		double deltaBeat = beatNum - curBpmInfo.beatNum;
		double deltaTime = deltaBeat * 60000.0 / curBpmInfo.bpm;
		int noteTime = curBpmInfo.time + deltaTime;

		//write to dstNoteObj
		QJsonObject dstNoteObj;
		dstNoteObj["column"] = noteObj["column"].toInt();
		dstNoteObj["time"] = noteTime;

		//deal with endTime
		if (noteObj.contains("endbeat"))
		{
			const auto& endBeatArr = noteObj["endbeat"].toArray();
			double beatNum = getBeatNum(endBeatArr);

			int bpmIndex = 0;
			for (qsizetype i = 0; i < bpmInfoVec.size(); i++)
			{
				if (beatNum >= bpmInfoVec[i].beatNum)
				{
					bpmIndex = i;
				}
				else
				{
					break;
				}
			}

			bpmInfo bpmInfo = bpmInfoVec[bpmIndex];
			double deltaBeat = beatNum - bpmInfo.beatNum;
			double deltaTime = deltaBeat * 60000.0 / bpmInfo.bpm;
			int endNoteTime = bpmInfo.time + deltaTime;
			dstNoteObj["endTime"] = endNoteTime;
		}

		//add dstNoteObj to dstNoteArray
		dstNoteArray.append(dstNoteObj);
	}
	// 3. Write to dstJsonObj
	dstJsonObj["note"] = dstNoteArray;

	//Write offset to dstJsonObj
	dstJsonObj["offset"] = miscObj["offset"];

	//Create a new directory in dstDir
	dstDir.cd(dstDirPath);
	QJsonObject songObj = metaObj["song"].toObject();
	QString dstDirName = songObj["title"].toString();
	dstDir.mkdir(dstDirName);
	dstDir.cd(dstDirName);
	//now dstDir is exactly in the new directory

	//Copy music
	QString musicName = miscObj["sound"].toString();
	QString musicSrcPath = srcDirPath + "/" + musicName;
	QString musicDstPath = dstDir.path() + "/" + musicName;
	if (!QFile::copy(musicSrcPath, musicDstPath))
	{
		qDebug() << "Warning: Copy music failed. sound: " << musicName << "\n";
	}
	//Copy background
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