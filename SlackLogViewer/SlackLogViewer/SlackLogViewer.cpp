#include "SlackLogViewer.h"
#include "GlobalVariables.h"
#include "FileDownloader.h"
#include "MessageListView.h"
#include "ReplyListView.h"
#include "MenuBar.h"
#include "UserListView.h"
#include <QHBoxLayout>
#include <QListView>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QTextBrowser>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStackedWidget>
#include <QMenu>
#include <QFileDialog>
#include <QErrorMessage>
#include <QApplication>
#include <QPushButton>
#include <QSplitter>
#include <QtConcurrent>
#include <QMessageBox>

SlackLogViewer::SlackLogViewer(QWidget* parent)
	: QMainWindow(parent), mImageView(nullptr), mTextView(nullptr), mSearchView(nullptr)
{
	gTempImage = std::make_unique<QImage>("Resources\\downloading.png");
	gDocIcon = std::make_unique<QImage>("Resources\\document.png");

	QWidget* central = new QWidget(this);
	QVBoxLayout* mainlayout = new QVBoxLayout();
	mainlayout->setContentsMargins(0, 0, 0, 0);
	mainlayout->setSpacing(0);
	{
		QMenu* menu = new QMenu();
		menu->setStyleSheet("QToolButton::menu-indicator { image: none; }"
							"QMenu { color: black; background-color: white; border: 1px solid grey; border-radius: 0px; }"
							"QMenu::item:selected { background-color: palette(Highlight); }");
		QAction* open = new QAction("Open");
		connect(open, &QAction::triggered, this, static_cast<void(SlackLogViewer::*)()>(&SlackLogViewer::OpenLogFile));
		menu->addAction(open);
		QMenu* recent = new QMenu("Open Recent");
		recent->setStyleSheet("QMenu { color: black; background-color: white; border: 1px solid grey; border-radius: 0px; }"
							  "QMenu::item:selected { background-color: palette(Highlight); }");
		{
			const auto& list = gSettings->value("History/LogFilePaths").toStringList();
			int s = gSettings->value("History/NumOfRecentLogFilePaths").toInt();
			mOpenRecentActions.resize(s);
			for (int i = 0; i < s; ++i)
			{
				if (i < list.size())
				{
					const QString& str = list[i];
					mOpenRecentActions[i] = recent->addAction(str);
				}
				else
				{
					mOpenRecentActions[i] = recent->addAction("");
					mOpenRecentActions[i]->setVisible(false);
				}
				connect(mOpenRecentActions[i], &QAction::triggered, [this, a = mOpenRecentActions[i]]() { OpenLogFile(a->text()); });
			}
		}
		menu->addMenu(recent);

		QAction* about = new QAction("About SlackLogViewer");
		connect(about, &QAction::triggered, this, &SlackLogViewer::OpenCredit);
		menu->addAction(about);
		QAction* close = new QAction("Exit");
		//connect(close, &QAction::triggered, this, &SlackLogViewer::Exit);
		connect(close, &QAction::triggered, qApp, &QCoreApplication::quit, Qt::QueuedConnection);
		menu->addAction(close);

		MenuBar* sb = new MenuBar(menu);
		sb->setFixedHeight(36);
		connect(sb, &MenuBar::SearchRequested, this, &SlackLogViewer::Search);
		connect(sb, &MenuBar::CacheAllRequested, this, &SlackLogViewer::CacheAllFiles);
		connect(sb, &MenuBar::ClearCacheRequested, this, &SlackLogViewer::ClearCache);

		mainlayout->addWidget(sb);
	}
	central->setLayout(mainlayout);

	mSplitter = new QSplitter();
	mSplitter->setHandleWidth(0);
	mSplitter->setOpaqueResize(false);
	mSplitter->setStyleSheet("background-color: white;");
	mainlayout->addWidget(mSplitter);
	{
		//Channel
		QWidget* w = new QWidget();
		QVBoxLayout* clayout = new QVBoxLayout();
		clayout->setContentsMargins(0, 0, 0, 0);
		clayout->setSpacing(0);
		//header
		QLabel* header = new QLabel("  Channels");
		header->setStyleSheet(
			"background-color: rgb(64, 14, 64);"
			"border-bottom: 1px solid rgb(128, 128, 128);"
			"border-top: 1px solid rgb(128, 128, 128);"
			"font-weight: bold;"
			"font-size: 16px;"
			"color: white;");
		header->setFixedHeight(gHeaderHeight);
		clayout->addWidget(header);

		mChannelView = new QListView();
		ChannelListModel* model = new ChannelListModel();
		mChannelView->setModel(model);
		mChannelView->setSelectionBehavior(QAbstractItemView::SelectRows);
		mChannelView->setSelectionMode(QAbstractItemView::SingleSelection);
		mChannelView->setWindowFlags(Qt::FramelessWindowHint);
		//item:selectedのフォントをboldにしようとしたが、どうも機能していないらしい。多分バグ。
		mChannelView->setStyleSheet(
			"QListView, QScrollBar {"
			"background-color: rgb(64, 14, 64);"
			"border: 0px;"
			"font-size: 16px; color: rgb(176, 176, 176); }"
			"QListView::item:selected {"
			"color: white;"
			//"font-weight: bold;"
			"}");
		connect(mChannelView, &QListView::clicked, [this](const QModelIndex& index) { SetChannel(index.row()); });
		clayout->addWidget(mChannelView);
		w->setLayout(clayout);
		mSplitter->addWidget(w);
	}
	{
		//Message
		QWidget* w = new QWidget();
		QVBoxLayout* mlayout = new QVBoxLayout();
		mlayout->setContentsMargins(0, 0, 0, 0);
		mlayout->setSpacing(0);
		mMessageHeader = new MessageHeaderWidget();
		mlayout->addWidget(mMessageHeader);
		connect(mMessageHeader, &MessageHeaderWidget::JumpRequested, this, &SlackLogViewer::JumpToPage);

		connect(mSplitter, &QSplitter::splitterMoved,
				[this](int pos, int index)
				{
					int i = mChannelPages->currentIndex();
					if (i < 0) return;
					if (i < gChannelVector.size())
					{
						MessageListView* m = static_cast<MessageListView*>(mChannelPages->currentWidget());
						m->reset();
					}
					else if (mChannelPages->currentWidget() == mSearchView)
					{
						mSearchView->GetView()->reset();
					}
				});
		mStack = new QStackedWidget();
		mStack->setStyleSheet("background-color: white;");
		{
			mChannelPages = new QStackedWidget();
			mStack->addWidget(mChannelPages);
		}
		{
			mImageView = new ImageView();
			connect(mImageView, &ImageView::Closed, this, &SlackLogViewer::CloseDocument);
			mStack->addWidget(mImageView);
		}
		{
			mTextView = new TextView();
			connect(mTextView, &TextView::Closed, this, &SlackLogViewer::CloseDocument);
			mStack->addWidget(mTextView);
		}
		{
			mPDFView = new PDFView();
			connect(mPDFView, &PDFView::Closed, this, &SlackLogViewer::CloseDocument);
			mStack->addWidget(mPDFView);
		}
		{
			mSearchView = new SearchResultListView();
			mSearchView->GetView()->setModel(new SearchResultModel(mSearchView->GetView()));
			mSearchView->GetView()->setItemDelegate(new SearchResultDelegate(mSearchView->GetView()));
			mStack->addWidget(mSearchView);
			connect(mSearchView, &SearchResultListView::Closed, this, &SlackLogViewer::CloseDocument);
		}
		mlayout->addWidget(mStack);
		w->setLayout(mlayout);
		mSplitter->addWidget(w);
	}
	{
		//Thread
		QWidget* w = new QWidget();
		QVBoxLayout* tlayout = new QVBoxLayout();
		tlayout->setContentsMargins(0, 0, 0, 0);
		tlayout->setSpacing(0);
		//header
		mRightStackLabel = new QLabel("  Thread");
		mRightStackLabel->setStyleSheet(
			"background-color: white;"
			"border-bottom: 1px solid rgb(128, 128, 128);"
			"border-top: 1px solid rgb(128, 128, 128);"
			"border-left: 1px solid rgb(128, 128, 128);"
			"font-weight: bold;"
			"font-size: 16px;"
			"color: black;");
		mRightStackLabel->setFixedHeight(gHeaderHeight);
		tlayout->addWidget(mRightStackLabel);

		mRightStack = new QStackedWidget();
		mRightStack->setStyleSheet(
			"background-color: white;"
			"border: 0px;"
			"border-left: 1px solid rgb(128, 128, 128);");

		//Thread View
		mThread = new ReplyListView();
		//mThread->setMaximumWidth(500);
		mThread->setModel(new ReplyListModel(mThread));
		mThread->setItemDelegate(new ReplyDelegate(mThread));
		connect(mSplitter, &QSplitter::splitterMoved,
				[this](int pos, int index)
				{
					if (index == 2)
						mThread->reset();
				});
		mThread->setStyleSheet("border: 0px;");
		mRightStack->addWidget(mThread);

		//user Profile
		mProfile = new UserProfileWidget();
		mProfile->setStyleSheet("border: 0px;");
		mRightStack->addWidget(mProfile);

		tlayout->addWidget(mRightStack);
		w->setLayout(tlayout);
		mSplitter->addWidget(w);
	}
	mSplitter->setSizes({ 270, 1200, 500 });

	setCentralWidget(central);
	setWindowState(Qt::WindowMaximized);

	restoreGeometry(gSettings->value("Geometry/MainWindow").toByteArray());
	restoreState(gSettings->value("WindowState/MainWindow").toByteArray());

	mSplitter->restoreGeometry(gSettings->value("Geometry/Splitter").toByteArray());
	mSplitter->restoreState(gSettings->value("WindowState/Splitter").toByteArray());
}

void SlackLogViewer::LoadUsers()
{
	gUsers.clear();
	//emptyuserも初期化しておく。
	QFile icon("Resources\\batsu.png");
	if (icon.exists())
	{
		icon.open(QIODevice::ReadOnly);
		gEmptyUser = std::make_unique<User>();
		gEmptyUser->SetUserIcon(icon.readAll());
	}
	QJsonDocument users = LoadJsonFile(gSettings->value("History/LastLogFilePath").toString() + "\\users.json");
	const QJsonArray& arr = users.array();
	for (const auto& u : arr)
	{
		const QJsonObject& o = u.toObject();
		User user(o);
		auto it = gUsers.insert(user.GetID(), std::move(user));

		QFile icon("Cache\\" + gWorkspace + "\\Icon\\" + user.GetID());
		if (icon.exists())
		{
			icon.open(QIODevice::ReadOnly);
			it.value().SetUserIcon(icon.readAll());
		}
		else
		{
			FileDownloader* fd = new FileDownloader(user.GetIconUrl());
			//スロットが呼ばれるタイミングは遅延しているので、
			//gUsersに格納されたあとのUserオブジェクトを渡しておく必要があるはず。
			QObject::connect(fd, &FileDownloader::Downloaded, [fd, puser = &it.value()]()
			{
				QByteArray image = fd->GetDownloadedData();
				QFile o("Cache\\" + gWorkspace + "\\Icon\\" + puser->GetID());
				o.open(QIODevice::WriteOnly);
				o.write(image);
				puser->SetUserIcon(fd->GetDownloadedData());
				fd->deleteLater();
			});
			QObject::connect(fd, &FileDownloader::DownloadFailed, [fd, puser = &it.value()]()
			{
				QFile icon("Resources\\batsu.png");
				icon.open(QIODevice::ReadOnly);
				puser->SetUserIcon(icon.readAll());
				fd->deleteLater();
			});
			//gDownloadSerializer.AddRequest(fd);
		}
	}
}

void SlackLogViewer::LoadChannels()
{
	for (int i = mChannelPages->count(); i > 0; i--)
	{
		QWidget* widget = mChannelPages->widget(0);
		mChannelPages->removeWidget(widget);
		widget->deleteLater();
	}
	gChannelVector.clear();
	QJsonDocument users = LoadJsonFile(gSettings->value("History/LastLogFilePath").toString() + "\\channels.json");
	const QJsonArray& arr = users.array();
	ChannelListModel* model = static_cast<ChannelListModel*>(mChannelView->model());
	model->insertRows(0, arr.size());
	for (size_t i = 0; i < arr.size(); ++i)
	{
		auto& c = arr[i];
		const QString& id = c["id"].toString();
		const QString& name = c["name"].toString();
		bool found = false;
		for (const auto& n : gHiddenChannels)
		{
			if (name == n)
			{
				found = true;
				break;
			}
		}
		if (found) continue;
		model->SetChannelInfo(i, id, name);
	}

	for (auto ch : gChannelVector)
	{
		MessageListView* mw = new MessageListView();
		mw->setModel(new MessageListModel(mw));
		//auto * p  =mw->itemDelegate();
		mw->setItemDelegate(new MessageDelegate(mw));
		mChannelPages->addWidget(mw);
		connect(mw, &MessageListView::CurrentPageChanged, mMessageHeader, &MessageHeaderWidget::SetCurrentPage);
	}
}
void SlackLogViewer::UpdateRecentFiles()
{
	QStringList ws = gSettings->value("History/LogFilePaths").toStringList();
	int s = gSettings->value("History/NumOfRecentLogFilePaths").toInt();
	int x = std::min(s, ws.size());
	int i = 0;
	for (; i < x; ++i)
	{
		mOpenRecentActions[i]->setText(ws[i]);
		mOpenRecentActions[i]->setVisible(true);
	}
	for (; i < s; ++i)
	{
		mOpenRecentActions[i]->setVisible(false);
	}
}
void SlackLogViewer::OpenLogFile()
{
	QString path = gSettings->value("History/LastLogFilePath").toString();
	path = QFileDialog::getExistingDirectory(this, "Open", path);
	OpenLogFile(path);
}
void SlackLogViewer::OpenLogFile(const QString& path)
{
	if (path.isEmpty()) return;
	{
		QFile file(path + "\\channels.json");
		if (!file.exists())
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("channels.json does not exist.");
			return;
		}
	}
	{
		QFile file(path + "\\users.json");
		if (!file.exists())
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("users.json does not exist.");
			return;
		}
	}

	{
		//ReplyListViewの削除
		mThread->Close();
	}
	{
		//SearchResultViewの削除
		mSearchView->Close();
	}


	QDir dir = path;
	gWorkspace = dir.dirName();

	//cache用フォルダの作成
	{
		QDir dir;
		if (!dir.exists("Cache\\" + gWorkspace) && !dir.mkdir("Cache\\" + gWorkspace))
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("cannot make cache folder.");
			return;
		}
		if (!dir.exists("Cache\\" + gWorkspace + "\\Text") && !dir.mkdir("Cache\\" + gWorkspace + "\\Text"))
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("cannot make cache folder.");
			return;
		}
		if (!dir.exists("Cache\\" + gWorkspace + "\\Image") && !dir.mkdir("Cache\\" + gWorkspace + "\\Image"))
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("cannot make cache folder.");
			return;
		}
		if (!dir.exists("Cache\\" + gWorkspace + "\\PDF") && !dir.mkdir("Cache\\" + gWorkspace + "\\PDF"))
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("cannot make cache folder.");
			return;
		}
		if (!dir.exists("Cache\\" + gWorkspace + "\\Others") && !dir.mkdir("Cache\\" + gWorkspace + "\\Others"))
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("cannot make cache folder.");
			return;
		}
		if (!dir.exists("Cache\\" + gWorkspace + "\\Icon") && !dir.mkdir("Cache\\" + gWorkspace + "\\Icon"))
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("cannot make cache folder.");
			return;
		}
	}
	gSettings->setValue("History/LastLogFilePath", path);
	QStringList ws = gSettings->value("History/LogFilePaths").toStringList();
	auto it = std::find(ws.begin(), ws.end(), path);
	if (it == ws.end())
	{
		ws.push_front(path);
	}
	else
	{
		ws.erase(it);
		ws.push_front(path);
	}
	{
		size_t s = gSettings->value("History/NumOfRecentLogFilePaths").toInt();
		while (ws.size() > s) ws.pop_back();
	}
	gSettings->setValue("History/LogFilePaths", ws);

	LoadChannels();
	LoadUsers();
	UpdateRecentFiles();

	SetChannel(0);
}
void SlackLogViewer::OpenOption()
{

}
void SlackLogViewer::Exit()
{
	QApplication::quit();
}
void SlackLogViewer::SetChannel(int row)
{
	ReplyListModel* tmodel = static_cast<ReplyListModel*>(mThread->model());
	if (!tmodel->IsEmpty() && row != tmodel->GetChannelIndex()) tmodel->Close();
	MessageListView* m = static_cast<MessageListView*>(mChannelPages->widget(row));
	const Channel& ch = gChannelVector[row];
	//setCurrentIndexはCreateより先に呼ばなければならない。
	//でないと、MessagePagesにwidthがフィットされない状態でdelegateのsizeHintが呼ばれてしまうらしい。
	mChannelPages->setCurrentIndex(row);
	mChannelView->setCurrentIndex(mChannelView->model()->index(row, 0));
	mStack->setCurrentWidget(mChannelPages);
	if (!m->IsConstructed())
	{
		m->Construct(ch.GetName());
	}
	int npages = m->GetNumOfPages();
	int page = m->GetCurrentPage();
	auto it = std::find(mBrowsedChannels.begin(), mBrowsedChannels.end(), m);
	{
		if (it == mBrowsedChannels.end())
		{
			mBrowsedChannels.push_front(m);
		}
		else
		{
			mBrowsedChannels.erase(it);
			mBrowsedChannels.push_front(m);
		}
		size_t s = gSettings->value("History/NumOfChannelStorage").toInt();
		while (mBrowsedChannels.size() > s)
		{
			auto* p = mBrowsedChannels.back();
			p->Clear();
			mBrowsedChannels.pop_back();
		}
	}
	mMessageHeader->Open(ch.GetName(), npages, page);
}
void SlackLogViewer::SetChannelAndIndex(int ch, int messagerow, int parentrow)
{
	SetChannel(ch);
	MessageListView* m = static_cast<MessageListView*>(mChannelPages->widget(ch));
	QAbstractItemModel* model = m->model();
	if (parentrow == -1)
	{
		//メッセージの場合
		m->ScrollToRow(messagerow);
	}
	else
	{
		//返信の場合はまずチャンネルを移動
		m->ScrollToRow(parentrow);
		//その後返信を表示
		OpenThread(m->GetMessages()[parentrow].get());
		QAbstractItemModel* tmodel = mThread->model();
		//rowは+2する。index==0は親メッセージ、index==1はボーターなので。
		mThread->ScrollToRow(messagerow + 2);
	}
}
void SlackLogViewer::JumpToPage(int page)
{
	int row = mChannelView->currentIndex().row();
	if (row == -1) return;
	MessageListView* m = static_cast<MessageListView*>(mChannelPages->widget(row));
	QAbstractItemModel* model = m->model();
	m->ScrollToRow(page * gSettings->value("NumOfMessagesPerPage").toInt());
}
void SlackLogViewer::OpenThread(const Message* m)
{
	static_cast<ReplyListModel*>(mThread->model())->Open(m, &m->GetThread()->GetReplies());
	mRightStack->setCurrentWidget(mThread);
	mRightStackLabel->setText("  Thread");
}
void SlackLogViewer::OpenImage(const ImageFile* i)
{
	mImageView->OpenImage(i);
	mStack->setCurrentWidget(mImageView);
}
void SlackLogViewer::OpenText(const AttachedFile* t)
{
	mTextView->OpenText(t);
	mStack->setCurrentWidget(mTextView);
}
void SlackLogViewer::OpenPDF(const AttachedFile* t)
{
	mPDFView->OpenPDF(t);
	mStack->setCurrentWidget(mPDFView);
}
void SlackLogViewer::OpenUserProfile(const User* u)
{
	mProfile->UpdateUserProfile(*u);
	mRightStack->setCurrentWidget(mProfile);
	mRightStackLabel->setText("  Profile");
}
void SlackLogViewer::Search(const QString& key, SearchMode mode)
{
	int row = mChannelView->currentIndex().row();
	if (row < 0 && mode.GetRangeMode() == SearchMode::CURRENTCH) return;

	if (mode.GetRangeMode() == SearchMode::ALLCHS) row = -1;

	size_t n = mSearchView->Search(row, mChannelPages, key, mode);
	mStack->setCurrentWidget(mSearchView);
	if (n != 0)
	{
		//n == 0、つまり要素が一つも見つからなかった場合は、Openを呼んではいけない。
		dynamic_cast<SearchResultModel*>(mSearchView->GetView()->model())->Open(&mSearchView->GetMessages());
		mSearchView->GetView()->scrollTo(mSearchView->GetView()->model()->index(0, 0));
	}
}
void SlackLogViewer::CacheAllFiles(CacheStatus::Channel ch, CacheStatus::Type type)
{
	//検索範囲
	std::vector<int> chs;
	if (ch == CacheStatus::ALLCHS)
	{
		chs.resize(gChannelVector.size());
		for (int i = 0; i < gChannelVector.size(); ++i) chs[i] = i;
	}
	else if (ch == CacheStatus::CURRENTCH)
	{
		chs = { mChannelView->currentIndex().row() };
	}
	QList<QFuture<CacheResult>> fs;
	for (int i : chs)
	{
		MessageListView* mes = static_cast<MessageListView*>(mChannelPages->widget(i));
		fs.append(QtConcurrent::run(::CacheAllFiles, i, mes, type));
	}
	size_t num_exist = 0;
	size_t num_downloaded = 0;
	size_t num_failure = 0;
	for (auto& f : fs)
	{
		f.waitForFinished();
		auto r = f.result();
		num_exist += r.num_exist;
		num_downloaded += r.num_downloaded;
		num_failure += r.num_failure;
	}
	QMessageBox b(this);
	b.setWindowTitle("Download result");
	b.setText(QString::number(num_downloaded) + " files downloaded.\n" +
			  QString::number(num_exist) + " files already in the cache.\n" +
			  QString::number(num_failure) + " files failed to download.");
	b.setDefaultButton(QMessageBox::Ok);
	b.exec();
}
void SlackLogViewer::ClearCache(CacheStatus::Type type)
{
	::ClearCache(type);
}

void SlackLogViewer::CloseDocument()
{
	mStack->setCurrentIndex(0);
}
void SlackLogViewer::OpenCredit()
{
	QDialog* d = new QDialog();
	QVBoxLayout* layout = new QVBoxLayout();
	d->setLayout(layout);
	d->setWindowFlags(d->windowFlags() & ~Qt::WindowContextHelpButtonHint);
	d->setWindowTitle("About SlackLogViewer");
	d->setFixedWidth(512);
	{
		QLabel* name = new QLabel(("SlackLogViewer ver " + gVersionInfo.GetVersionNumberStr()).c_str());
		name->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addWidget(name);
		QLabel* cr = new QLabel((gVersionInfo.GetCopyright()).c_str(), this);
		cr->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addWidget(cr);
		QLabel* git = new QLabel("GitHub : <a href=\"https://github.com/thayakawa-gh/SlackLogViewer\">https://github.com/thayakawa-gh/SlackLogViewer</a>");
		git->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
		git->setOpenExternalLinks(true);
		git->setTextFormat(Qt::RichText);
		layout->addWidget(git);
		QLabel* qt = new QLabel("This application uses the following libraries");
		qt->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addWidget(qt);
		QLabel* lgpl = new QLabel(
			"Qt Copyright (C) 2020 The Qt Company Ltd. distributed under the LGPL License<br/>"
			"<a href=\"https://opensource.org/licenses/LGPL-3.0\">https://opensource.org/licenses/LGPL-3.0</a><br/>"
			"emojicpp Copyright (c) 2018 Shalitha Suranga distributed under the MIT License<br/>"
			"<a href=\"https://opensource.org/licenses/MIT\">https://opensource.org/licenses/MIT</a>");
		lgpl->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
		lgpl->setOpenExternalLinks(true);
		lgpl->setWordWrap(true);
		lgpl->setTextFormat(Qt::RichText);
		layout->addWidget(lgpl);
	}
	d->exec();
	delete d;
}
void SlackLogViewer::closeEvent(QCloseEvent* event)
{
	gSettings->setValue("Geometry/MainWindow", saveGeometry());
	gSettings->setValue("WindowState/MainWindow", saveState());
	gSettings->setValue("Geometry/Splitter", mSplitter->saveGeometry());
	gSettings->setValue("WindowState/Splitter", mSplitter->saveState());
	QMainWindow::closeEvent(event);
}

QVariant ChannelListModel::data(const QModelIndex& index, int role) const
{
	if (role != Qt::DisplayRole) return QVariant();
	int row = index.row();
	const Channel* ch = static_cast<const Channel*>(index.internalPointer());
	return "  # " + ch->GetName();
}
int ChannelListModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid()) return (int)gChannelVector.size();
	return 0;
}
QModelIndex ChannelListModel::index(int row, int column, const QModelIndex& parent) const
{
	//parentがrootnodeでない場合は子ノードは存在しない。
	if (parent.isValid()) return QModelIndex();
	if (gChannelVector.size() <= row) return QModelIndex();
	return createIndex(row, 0, (void*)(&gChannelVector[row]));
}
bool ChannelListModel::insertRows(int row, int count, const QModelIndex& parent)
{
	beginInsertRows(parent, row, row + count - 1);
	gChannelVector.insert(row, count, Channel());
	endInsertRows();
	return true;
}
bool ChannelListModel::removeRows(int row, int count, const QModelIndex& parent)
{
	beginRemoveRows(parent, row, row + count - 1);
	gChannelVector.remove(row, count);
	endInsertRows();
	return true;
}

void ChannelListModel::SetChannelInfo(int row, const QString& id, const QString& name)
{
	gChannelVector[row].SetChannelInfo(id, name);
}

MessageHeaderWidget::MessageHeaderWidget()
	: mCurrentPage(-1), mNumOfPages(-1)
{
	setStyleSheet(
		"background-color: white;"
		"border-bottom: 1px solid rgb(128, 128, 128);"
		"border-top: 1px solid rgb(128, 128, 128);"
		"color: black;");

	QHBoxLayout* hlayout = new QHBoxLayout();
	hlayout->setContentsMargins(0, 0, 0, 0);

	//channel info
	mChannelLabel = new QLabel("  # Channel");
	mChannelLabel->setFixedHeight(gHeaderHeight - 2);//全体のwidgetのボーター分だけ高さを減らす。
	mChannelLabel->setStyleSheet("border: 0px;"
								 "font-weight: bold;"
								 "font-size: 16px;");
	hlayout->addWidget(mChannelLabel);

	hlayout->addStretch();

	//page number
	mPageNumberLayout = new QHBoxLayout();
	mPageNumberLayout->setContentsMargins(gSpacing, 0, gSpacing, 0);

	const int width = 24;

	mTop = new QPushButton("<<");
	mTop->setFixedWidth(width);
	mTop->setCursor(Qt::PointingHandCursor);
	mTop->setStyleSheet("border: 1px solid grey;");
	mTop->setToolTip("Jump to the first page");
	connect(mTop, SIGNAL(clicked()), this, SLOT(JumpToTop()));
	mPageNumberLayout->addWidget(mTop);

	mPrev = new QPushButton("<");
	mPrev->setFixedWidth(width);
	mPrev->setCursor(Qt::PointingHandCursor);
	mPrev->setStyleSheet("border: 1px solid grey;");
	mPrev->setToolTip("Go back to previous page");
	connect(mPrev, SIGNAL(clicked()), this, SLOT(JumpToPrev()));
	mPageNumberLayout->addWidget(mPrev);

	mNext = new QPushButton(">");
	mNext->setFixedWidth(width);
	mNext->setCursor(Qt::PointingHandCursor);
	mNext->setStyleSheet("border: 1px solid grey;");
	mNext->setToolTip("Go to the next page");
	connect(mNext, SIGNAL(clicked()), this, SLOT(JumpToNext()));
	mPageNumberLayout->addWidget(mNext);

	mBottom = new QPushButton(">>");
	mBottom->setFixedWidth(width);
	mBottom->setCursor(Qt::PointingHandCursor);
	mBottom->setStyleSheet("border: 1px solid grey;");
	mBottom->setToolTip("Jump to the last page");
	connect(mBottom, SIGNAL(clicked()), this, SLOT(JumpToBottom()));
	mPageNumberLayout->addWidget(mBottom);

	hlayout->addLayout(mPageNumberLayout, Qt::AlignBottom);

	mNumbers.reserve(msNumOfButtons);
	for (int i = 0; i < msNumOfButtons; ++i)
	{
		auto* p = new QPushButton();
		p->setFixedWidth(width);
		p->setCursor(Qt::PointingHandCursor);
		p->setStyleSheet("QPushButton:disabled { background-color: lightblue; } QPushButton{ border: 1px solid grey }");
		connect(p, &QPushButton::clicked, [this, i]() { JumpTo(i); });
		mNumbers.emplace_back(p);
	}
	setLayout(hlayout);
}

void MessageHeaderWidget::Open(const QString& ch, int npages, int currentpage)
{
	if (npages <= currentpage) throw std::exception();
	mNumOfPages = npages;
	QLayoutItem* item;
	Clear();
	int x = std::min(npages, msNumOfButtons);
	for (int i = 0; i < x; ++i)
	{
		auto* p = mNumbers[i];
		p->setText(QString::number(i));
		mPageNumberLayout->insertWidget(2 + i, p);
	}
	mChannelLabel->setText("  # " + ch);

	SetCurrentPage(currentpage);
}
void MessageHeaderWidget::SetCurrentPage(int page)
{
	//この関数は既にページボタンなどが適切に構築されていることが前提。
	//ボタンのページ番号を振り直す。
	int x = std::min(msNumOfButtons, mNumOfPages);
	if (page < msNumOfButtons / 2)
	{
		for (int i = 0; i < x; ++i)
		{
			auto* p = mNumbers[i];
			p->setText(QString::number(i));
			p->setVisible(true);
			p->setEnabled(true);
		}
		mNumbers[page]->setEnabled(false);
	}
	else if (mNumOfPages - page - 1 < msNumOfButtons / 2)
	{
		for (int i = 0; i < x; ++i)
		{
			auto* p = mNumbers[i];
			int j = mNumOfPages - x + i;
			p->setText(QString::number(j));
			p->setVisible(true);
			p->setEnabled(true);
		}
		mNumbers[page - (mNumOfPages - x)]->setEnabled(false);
	}
	else
	{
		//この分岐はボタン数x<5のときは決して呼ばれないので、
		//xを考慮する必要はない。
		for (int i = 0; i < x; ++i)
		{
			auto* p = mNumbers[i];
			int j = i + page - msNumOfButtons / 2;
			p->setText(QString::number(j));
			p->setVisible(true);
			p->setEnabled(true);
		}
		mNumbers[msNumOfButtons / 2]->setEnabled(false);
	}
	mCurrentPage = page;
}
//ジャンプ指示を出したからと言って、きちんとジャンプされるとは限らない。
//例えば、最後のページのメッセージ数が極端に少なかったりすると、スクロールできず、
//最後の一つ手前のページに飛ぶ、なんてこともある。
//そのため、mCurrentPageの更新はあくまでSetCurrentPage関数中で行う。この関数はMessageListViewのScrollToなどから呼ばれる。
void MessageHeaderWidget::JumpToTop() { emit JumpRequested(0); }
void MessageHeaderWidget::JumpToPrev() { emit JumpRequested(mCurrentPage - 1); }
void MessageHeaderWidget::JumpToNext() { emit JumpRequested(mCurrentPage + 1); }
void MessageHeaderWidget::JumpToBottom() { emit JumpRequested(mNumOfPages - 1); }
void MessageHeaderWidget::JumpTo(int index)
{
	//通常はmsNumOfButtons/2が現在のページを示すボタンのはずだが、
	//ページの端ではそうならないので、分岐する。
	if (mCurrentPage < msNumOfButtons / 2)
	{
		//msNumOfButtons/2==2とすると、
		//0～1ページにいる場合、
		//indexは単純にページ番号を表す。
		emit JumpRequested(index);
	}
	else if (mNumOfPages - mCurrentPage - 1 < msNumOfButtons / 2)
	{
		//ページの最後の方にいる場合。
		emit JumpRequested(mNumOfPages - (msNumOfButtons - index));
	}
	else
	{
		//ページの両端以外にいる場合、
		emit JumpRequested(mCurrentPage + index - msNumOfButtons / 2);
	}
}
void MessageHeaderWidget::Clear()
{
	for (auto* p : mNumbers)
	{
		mPageNumberLayout->removeWidget(p);
		p->hide();
	}
}

/*SplitterHandle::SplitterHandle(Qt::Orientation orientation, QSplitter* parent)
	: QSplitterHandle(orientation, parent), mPrevPos()
{}*/