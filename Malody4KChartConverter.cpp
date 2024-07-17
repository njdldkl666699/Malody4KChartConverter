#include "Malody4KChartConverter.h"
using ConvertStatus = Malody4KChartConverter::ConvertStatus;

Malody4KChartConverter::Malody4KChartConverter(QWidget* parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	connect(ui.pushButton_src, &QPushButton::clicked, this, &Malody4KChartConverter::selectSrcFile);
	connect(ui.pushButton_dst, &QPushButton::clicked, this, &Malody4KChartConverter::selectDstDir);
	connect(ui.pushButton_convert, &QPushButton::clicked, this, &Malody4KChartConverter::convert);
}

Malody4KChartConverter::~Malody4KChartConverter()
{
}

void Malody4KChartConverter::selectSrcFile()
{
	QFileDialog srcFileDialog(this, "Selece Malody 4K Charts", ".", "Malody Chart(*.mc)");
	srcFileDialog.setFileMode(QFileDialog::ExistingFiles);
	srcFileDialog.exec();
	srcFilePathList = srcFileDialog.selectedFiles();
	if (!srcFilePathList.isEmpty())
	{
		QString srcPathAll;
		QFileInfo* srcFilesInfo = new QFileInfo[srcFilePathList.size()];
		for (int i = 0; i < srcFilePathList.size(); i++)
		{
			srcFilesInfo[i].setFile(srcFilePathList[i]);
			srcPathAll += srcFilesInfo[i].fileName();
			if (i != srcFilePathList.size() - 1)
				srcPathAll += "; ";
		}
		ui.lineEdit_src->setText(srcPathAll);
	}
}

void Malody4KChartConverter::selectDstDir()
{
	QFileDialog dstFileDialog(this, "Select Directory", ".");
	dstFileDialog.setFileMode(QFileDialog::Directory);
	if (dstFileDialog.exec() == QFileDialog::Accepted)
	{
		dstDirPath = dstFileDialog.selectedFiles().first();
		ui.lineEdit_dst->setText(dstDirPath);
	}
}

void Malody4KChartConverter::convert()
{
	QString convertRlt;
	for (int i = 0; i < srcFilePathList.size(); i++)
	{
		if (convertSingle(srcFilePathList[i]) == ConvertFailed)
		{
			qDebug() << "Warning: Convert failed. File: " << srcFilePathList[i] << "\n";
			convertRlt += "Convert failed£º" + srcFilePathList[i] + "\n";
		}
	}
	convertRlt+= "Convert finished";
	ui.label_convertRlt->setText(convertRlt);
}

ConvertStatus Malody4KChartConverter::convertSingle(const QString& srcFilePath) const
{
	QFile srcFile(srcFilePath);
	if (!srcFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		qDebug() << "Error: Cannot open file.\n" << srcFilePath << "\n";
		return ConvertFailed;
	}
	else
	{
		QString wholeChart = srcFile.readAll();
		// Remove all whitespaces
		wholeChart.remove("\n");
		wholeChart.remove("\r");
		wholeChart.remove(" ");
		wholeChart.remove("\t");

		// Get destination path & Create file
		QFileInfo srcFileInfo(srcFilePath);
		QString dstPath = dstDirPath + "/" + srcFileInfo.fileName();
		dstPath.remove(".mc");
		dstPath.append("_converted.txt");
		QFile dstFile(dstPath);
		dstFile.open(QIODevice::Truncate | QIODevice::ReadWrite);
		QTextStream out(&dstFile);

		// Get bpm & Write to file
		int bpmPos = wholeChart.indexOf("\"bpm\":", 0);
		bpmPos += 6;
		int bpmEndPos = wholeChart.indexOf("}", bpmPos);
		QString bpm = wholeChart.mid(bpmPos, bpmEndPos - bpmPos);
		qDebug() << "bpm: " << bpm << "\n";
		out << "bpm," << bpm << "\n";

		// Get offset & Write to file
		int offsetPos = wholeChart.indexOf("\"offset\":", 0);
		offsetPos += 9;
		int offsetEndPos = wholeChart.indexOf(",", offsetPos);
		QString offset = wholeChart.mid(offsetPos, offsetEndPos - offsetPos);
		qDebug() << "offset: " << offset << "\n";
		out << "offset," << offset << "\n";

		// Get notes
		int notesPos = wholeChart.indexOf("\"note\":[{", 0);
		notesPos += 9;
		int notesEndPos = wholeChart.indexOf("},{\"beat\":[0,0,1]", notesPos);
		QString notes = wholeChart.mid(notesPos, notesEndPos - notesPos);
		QStringList notesList = notes.split("},{");

		// Seperate notes & Write to file
		for (int i = 0; i < notesList.size(); i++)
		{
			const QString& note = notesList.at(i);
			qDebug() << note << "\n";

			// Get beat
			int beatPos = note.indexOf("\"beat\":[", 0);
			beatPos += 8;
			int beatEndPos = note.indexOf("]", beatPos);
			QString beat = note.mid(beatPos, beatEndPos - beatPos);

			// Get column
			int columnPos = note.indexOf("\"column\":", 0);
			columnPos += 9;
			QString column = note.at(columnPos);

			// Get type & endbeat & Write to file
			int endbeatPos = note.indexOf("\"endbeat\":", 0);
			if (endbeatPos == -1)
			{
				// Tap note
				out << "0;" << beat << ";" << column << "\n";
			}
			else
			{
				// Hold note
				endbeatPos += 11;
				int endbeatEndPos = note.indexOf("]", endbeatPos);
				QString endbeat = note.mid(endbeatPos, endbeatEndPos - endbeatPos);
				out << "1;" << beat << ";" << endbeat << ";" << column << "\n";
			}
		}

		// Close files
		dstFile.close();
		srcFile.close();
		return ConvertSuccess;
	}
}