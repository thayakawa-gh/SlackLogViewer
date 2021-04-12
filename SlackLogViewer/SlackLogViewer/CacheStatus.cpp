#include <QToolButton>
#include <QPushButton>
#include <QMenu>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFile>
#include <QDir>
#include "CacheStatus.h"
#include "GlobalVariables.h"
#include "MessageListView.h"
#include "FileDownloader.h"

CacheStatus::CacheStatus(QWidget* parent)
	: mParent(parent)
{
	this->setFixedSize(300, 160);
	QVBoxLayout* layout = new QVBoxLayout();
	QLabel* title = new QLabel("Cache status");
	title->setAlignment(Qt::AlignCenter);
	title->setStyleSheet("color: black; background-color: white; border: 0px solid grey; font-weight: bold;");
	layout->addWidget(title);

	QGridLayout* status = new QGridLayout();
	int count = 0;
	for (const auto& x : msTypeList)
	{
		QLabel* label = new QLabel(x);
		label->setStyleSheet("border: 0px solid grey;");
		QLabel* num = new QLabel();
		num->setAlignment(Qt::AlignRight);
		mNum.push_back(num);
		QLabel* byte = new QLabel();
		byte->setAlignment(Qt::AlignRight);
		mSize.push_back(byte);

		QToolButton* dl = new QToolButton();
		dl->setCursor(Qt::PointingHandCursor);
		dl->setStyleSheet("QToolButton::menu-indicator { image: none; border: 0px; }");
		dl->setIcon(QIcon("Resources\\download.png"));
		dl->setToolTip("Download all files in");
		dl->setPopupMode(QToolButton::InstantPopup);
		QMenu* dlmenu = new QMenu();
		dlmenu->setStyleSheet("QMenu { color: black; background-color: white; border: 1px solid grey; border-radius: 0px; }"
							  "QMenu::item:selected { background-color: palette(Highlight); }");
		QAction* dlall = new QAction();
		dlall->setText("All channels");
		connect(dlall, &QAction::triggered, [this, count]() { CacheAllRequested(ALLCHS, (Type)count); });
		dlmenu->addAction(dlall);
		QAction* dlcur = new QAction();
		dlcur->setText("Current channel");
		connect(dlcur, &QAction::triggered, [this, count]() { CacheAllRequested(CURRENTCH, (Type)count); });
		dlmenu->addAction(dlcur);
		dl->setMenu(dlmenu);

		QPushButton* cl = new QPushButton();
		cl->setIcon(QIcon("Resources\\clear.svg"));
		cl->setCursor(Qt::PointingHandCursor);
		cl->setToolTip("Clear cache");
		connect(cl, &QPushButton::clicked, [this, count]() { ClearCacheRequested((Type)count); });

		status->addWidget(label, count, 0);
		status->addWidget(num, count, 1);
		status->addWidget(byte, count, 2);
		status->addWidget(cl, count, 3);
		status->addWidget(dl, count, 4);
		++count;
	}
	status->setColumnStretch(0, 1);
	status->setColumnStretch(1, 1);
	status->setColumnStretch(2, 1);
	layout->addLayout(status);

	this->setLayout(layout);
	setStyleSheet("color: black; background-color: white; border: 1px solid grey; border-radius: 0px;");
}
void CacheStatus::showEvent(QShowEvent* event)
{
	size_t num_all = 0;
	qint64 size_all = 0;
	auto print_size = [](QLabel* byte, qint64 size)
	{
		double exp = std::log(size) / std::log(1024);
		if (exp < 1) byte->setText(QString::number(size) + " B");
		else if (exp < 2) byte->setText(QString::number(size / 1024., 'g', 4) + " KB");
		else if (exp < 3) byte->setText(QString::number(size / std::pow(1024, 2), 'g', 4) + " MB");
		else if (exp < 4) byte->setText(QString::number(size / std::pow(1024, 3), 'g', 4) + " GB");
		else if (exp < 5) byte->setText(QString::number(size / std::pow(1024, 4), 'g', 4) + " TB");
		else byte->setText("inf");
	};
	for (size_t i = 0; i < msTypeList.size(); ++i)
	{
		auto& x = msTypeList[i];
		QLabel* num = mNum[i];
		QLabel* byte = mSize[i];
		if (x != "All")
		{
			//num of files
			QDir dir("Cache\\" + gWorkspace + "\\" + x);
			auto infos = dir.entryInfoList(QDir::Files);
			num->setText(QString::number(infos.size()));
			num_all += infos.size();
			//total of file size
			qint64 size = 0;
			for (auto& i : infos)
				size += i.size();
			size_all += size;
			print_size(byte, size);
		}
		else
		{
			num->setText(QString::number(num_all));
			print_size(byte, size_all);
		}
	}
	QFrame::showEvent(event);
}

CacheResult CacheAllFiles(int ch, MessageListView* mes, CacheStatus::Type type)
{
	if (!mes->IsConstructed()) mes->Construct(gChannelVector[ch].GetName());
	size_t downloaded = 0;
	size_t exist = 0;
	size_t failure = 0;
	size_t* res_set[3] = { &downloaded, &exist, &failure };
	bool text = type == CacheStatus::TEXT || type == CacheStatus::ALL;
	bool image = type == CacheStatus::IMAGE || type == CacheStatus::ALL;
	bool pdf = type == CacheStatus::PDF || type == CacheStatus::ALL;
	bool others = type == CacheStatus::OTHERS || type == CacheStatus::ALL;

	//ダウンロード成功で0、既にファイルが有れば1、ダウンロード失敗で2を返す。
	//関係ないファイルだった場合（ダウンロード指定がtextなのに保有ファイルがimageなど）は3を返す。
	auto dl = [text, image, pdf, others](std::unique_ptr<AttachedFile>& f)
	{
		if (text && f->IsText() ||
			image && f->IsImage() ||
			pdf && f->IsPDF() ||
			others && f->IsOther())
		{
			QString path = "Cache\\" + gWorkspace;
			if (f->IsText()) path += "\\Text\\" + f->GetID();
			else if (f->IsImage()) path += "\\Image\\" + f->GetID();
			else if (f->IsPDF()) path += "\\PDF\\" + f->GetID();
			else if (f->IsOther()) path += "\\Others\\" + f->GetID();
			else throw std::exception();
			{
				QFile file(path);
				if (file.exists())
				{
					return 1;
				}
			}
			FileDownloader* fd = new FileDownloader(f->GetUrl());
			bool res = fd->Wait();
			if (!res)
			{
				delete fd;
				return 2;
			}
			QFile o(path);
			o.open(QIODevice::WriteOnly);
			o.write(fd->GetDownloadedData());
			o.close();
			delete fd;
			return 0;
		}
		return 3;
	};
	for (auto& m : mes->GetMessages())
	{
		for (auto& f : m->GetFiles())
		{
			int res = 0;
			for (int i = 0; i < 3; ++i)
			{
				res = dl(f);
				if (res != 2) break;
			}
			if (res == 3) continue;
			++(*res_set[res]);
		}
		const Thread* th = m->GetThread();
		if (!th) continue;
		for (auto& t : th->GetReplies())
		{
			for (auto& tf : t->GetFiles())
			{
				int res = 0;
				for (int i = 0; i < 3; ++i)
				{
					res = dl(tf);
					if (res != 2) break;
				}
				if (res == 3) continue;
				++(*res_set[res]);
			}
		}
	}
	return { downloaded, exist, failure };
}
void ClearCache(CacheStatus::Type type)
{
	auto del = [](const char* type)
	{
		QString path = "Cache\\" + gWorkspace + "\\" + type;
		QDir dir(path);
		auto infos = dir.entryInfoList(QDir::Files);
		for (auto& f : infos)
		{
			QFile::remove(f.filePath());
		}
	};
	if (type == CacheStatus::TEXT   || type == CacheStatus::ALL) del("Text");
	if (type == CacheStatus::IMAGE  || type == CacheStatus::ALL) del("Image");
	if (type == CacheStatus::PDF    || type == CacheStatus::ALL) del("PDF");
	if (type == CacheStatus::OTHERS || type == CacheStatus::ALL) del("Others");
}