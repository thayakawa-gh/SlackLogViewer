#include "SlackLogViewer.h"
#include "GlobalVariables.h"
#include "FileDownloader.h"
#include "MessageListView.h"
#include "ReplyListView.h"
#include "MenuBar.h"
#include "UserListView.h"
#include <QHBoxLayout>
#include <QListView>
#include <QTreeView>
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
#include <QException>

SlackLogViewer::SlackLogViewer(QWidget* parent)
	: QMainWindow(parent), mImageView(nullptr), mTextView(nullptr), mSearchView(nullptr)
{
	gTempImage = std::make_unique<QImage>(ResourcePath("downloading.png"));
	gDocIcon = std::make_unique<QImage>(ResourcePath("document.png"));

	QWidget* central = new QWidget(this);
	QVBoxLayout* mainlayout = new QVBoxLayout();
	mainlayout->setContentsMargins(0, 0, 0, 0);
	mainlayout->setSpacing(0);
	{
		QMenu* menu = new QMenu();
		menu->setStyleSheet("QToolButton::menu-indicator { image: none; }"
							"QMenu { color: black; background-color: white; border: 1px solid grey; border-radius: 0px; }"
							"QMenu::item:selected { background-color: palette(Highlight); }");
		QMenu* open = new QMenu("Open");
		QAction* open_folder = new QAction("folder");
		connect(open_folder, &QAction::triggered, this, &SlackLogViewer::OpenLogFileFolder);
		open->addAction(open_folder);
		QAction* open_zip = new QAction("zip file");
		connect(open_zip, &QAction::triggered, this, &SlackLogViewer::OpenLogFileZip);
		open->addAction(open_zip);
		menu->addMenu(open);
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
		gChannelParentVector.resize((int)Channel::END_CH);
		gChannelParentVector[(int)Channel::CHANNEL].SetChannelInfo(Channel::CHANNEL, "", "", {});
		gChannelParentVector[(int)Channel::DIRECT_MESSAGE].SetChannelInfo(Channel::DIRECT_MESSAGE, "", "", {});
		gChannelParentVector[(int)Channel::GROUP_MESSAGE].SetChannelInfo(Channel::GROUP_MESSAGE, "", "", {});
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

		mChannelView = new QTreeView();
		ChannelTreeModel* model = new ChannelTreeModel();
		mChannelView->setModel(model);
		mChannelView->setSelectionBehavior(QAbstractItemView::SelectRows);
		mChannelView->setSelectionMode(QAbstractItemView::SingleSelection);
		mChannelView->setWindowFlags(Qt::FramelessWindowHint);
		mChannelView->setHeaderHidden(true);
		//item:selectedのフォントをboldにしようとしたが、どうも機能していないらしい。多分バグ。
		mChannelView->setStyleSheet(
			"QTreeView, QScrollBar {"
			"background-color: rgb(64, 14, 64);"
			"border: 0px;"
			"font-size: 16px; color: rgb(176, 176, 176); }"
			"QTreeView::item:selected {"
			"color: white;"
			"font-weight: bold;"
			"}"
		);
		connect(mChannelView, &QTreeView::clicked,
				[this](const QModelIndex& index)
				{
					if (!index.parent().isValid()) return;
					const Channel* ch = static_cast<const Channel*>(index.internalPointer());
					Channel::Type type = ch->GetType();
					if (type == Channel::CHANNEL) SetChannel(type, index.row());
					else if (type == Channel::DIRECT_MESSAGE) SetChannel(type, index.row());
					else if (type == Channel::GROUP_MESSAGE) SetChannel(type, index.row());
				});
		mChannelView->expandAll();
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
				[this](int /*pos*/, int /*index*/)
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
				[this](int /*pos*/, int index)
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

void SlackLogViewer::ClearUsersAndChannels()
{
	//ユーザー、チャンネル、プライベートやダイレクトメッセージなど含めて全てを削除する。

	//まずはCHANNEL、DIRECT、GROUP_MESSAGEのすべてを削除。
	ChannelTreeModel* model = static_cast<ChannelTreeModel*>(mChannelView->model());
	for (int i = 0; i < (int)Channel::END_CH; ++i)
	{
		QModelIndex index = model->index(i, 0, QModelIndex());
		int count = model->rowCount(index);
		if (count == 0) continue;
		model->removeRows(0, model->rowCount(index), index);
	}

	for (int i = mChannelPages->count(); i > 0; i--)
	{
		QWidget* widget = mChannelPages->widget(0);
		mChannelPages->removeWidget(widget);
		widget->deleteLater();
	}

	//最後にユーザー情報の削除。
	//channel viewのremoveRowsがgUsersへアクセスすることがあるため、
	//channel viewよりも後に削除すること。
	gUsers.clear();
	//emptyuserも初期化しておく。
	QFile batsuicon(ResourcePath("batsu.png"));
	if (batsuicon.exists())
	{
		batsuicon.open(QIODevice::ReadOnly);
		gEmptyUser = std::make_unique<User>();
		gEmptyUser->SetUserIcon(batsuicon.readAll());
	}
}

void SlackLogViewer::LoadUsers()
{
	QJsonDocument users = LoadJsonFile(gSettings->value("History/LastLogFilePath").toString(), "users.json");
	const QJsonArray& arr = users.array();
	for (const auto& u : arr)
	{
		const QJsonObject& o = u.toObject();
		User user(o);
		auto it = gUsers.insert(user.GetID(), std::move(user));

		QFile icon(IconPath(user.GetID()));
		if (icon.exists())
		{
			icon.open(QIODevice::ReadOnly);
			it.value().SetUserIcon(icon.readAll());
		}
		else
		{
			FileDownloader* fd = new FileDownloader();
			fd->RequestDownload(user.GetIconUrl());
			//スロットが呼ばれるタイミングは遅延しているので、
			//gUsersに格納されたあとのUserオブジェクトを渡しておく必要があるはず。
			QObject::connect(fd, &FileDownloader::Downloaded, [fd, puser = &it.value()]()
			{
				QByteArray image = fd->GetDownloadedData();
				QFile o(IconPath(puser->GetID()));
				o.open(QIODevice::WriteOnly);
				o.write(image);
				puser->SetUserIcon(fd->GetDownloadedData());
				fd->deleteLater();
			});
			QObject::connect(fd, &FileDownloader::DownloadFailed, [fd, puser = &it.value()]()
			{
				QFile icon(ResourcePath("batsu.png"));
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
	//ここから新しいチャンネル情報の読み込み。
	QJsonDocument channels = LoadJsonFile(gSettings->value("History/LastLogFilePath").toString(), "channels.json");
	if (channels.isNull())
	{
		return;
		//public channelが一つもないのは想定外であるので、強制終了する。
		//QMessageBox q;
		//q.setText(QString::fromStdString("Error : This workspace has no public channels."));
		//q.exec();
		//exit(EXIT_FAILURE);
	}
	const QJsonArray& arr = channels.array();
	if (arr.size() == 0) return;

	ChannelTreeModel* model = static_cast<ChannelTreeModel*>(mChannelView->model());
	QModelIndex index = model->index((int)Channel::CHANNEL, 0, QModelIndex());
	model->insertRows(0, arr.size(), index);

	int row = 0;
	for (auto c : arr)
	{
		const QString& id = c.toObject()["id"].toString();
		const QString& name = c.toObject().value("name").toString();
		bool is_private = (c.toObject().contains("is_private") && c.toObject()["is_private"].toBool() == true);
		const QJsonArray& arr_ = c.toObject()["members"].toArray();
		QVector<QString> members(arr_.size());
		for (int i = 0; i < members.size(); ++i)
		{
			members[i] = arr_[i].toString();
		}
		model->SetChannelInfo(row, is_private, id, name, members);
		++row;
	}

	for (int i = 0; i < arr.size(); ++i)
	{
		MessageListView* mw = new MessageListView();
		mw->setModel(new MessageListModel(mw));
		mw->setItemDelegate(new MessageDelegate(mw));
		mChannelPages->addWidget(mw);
		connect(mw, &MessageListView::CurrentPageChanged, mMessageHeader, &MessageHeaderWidget::SetCurrentPage);
	}
}
void SlackLogViewer::LoadDirectMessages()
{
	QJsonDocument dms = LoadJsonFile(gSettings->value("History/LastLogFilePath").toString(), "dms.json");
	if (dms.isNull()) return;
	const QJsonArray& arr = dms.array();
	//どうもファイルが存在する場合はisNullは空配列でもfalseになるらしい。arrのサイズもチェックする。
	//dms.jsonが空配列という状況が今ひとつ想像できないが、
	if (arr.size() == 0) return;
	ChannelTreeModel* model = static_cast<ChannelTreeModel*>(mChannelView->model());
	QModelIndex parent = model->index((int)Channel::DIRECT_MESSAGE, 0, QModelIndex());
	model->insertRows(0, arr.size(), parent);

	int row = 0;
	for (auto dm : arr)
	{
		FindKeyAs find_as("dms.json", row);
		auto o = dm.toObject();
		const QString& id = find_as.string(o, "id");
		const QJsonArray& arr_ = find_as.array(o, "members");
		//QString n = gUsers.find(arr_.first().toString()).value().GetName();
		if (arr_.size() < 2) throw FatalError("The size of array \"members\" is less than 2.");
		const QJsonValue& v = arr_[0];
		if (!v.isString()) throw FatalError("Non-string value exists in the array \"members\" of \"dms.json\".");
		auto it = gUsers.find(v.toString());
		if (it == gUsers.end()) throw FatalError("The user id \"" + v.toString() +"\" not found in users.json.");
		const QString& n = it.value().GetName();
		QVector<QString> members(arr_.size());
		for (int i = 0; i < members.size(); ++i)
		{
			if (!arr_[i].isString()) throw FatalError("Non-string value exists in the array \"members\" of \"dms.json\".");
			members[i] = arr_[i].toString();
		}
		model->SetDMUserInfo(row, id, n, members);
		++row;
	}

	for (int i = 0; i < arr.size(); ++i)
	{
		MessageListView* mw = new MessageListView();
		mw->setModel(new MessageListModel(mw));
		mw->setItemDelegate(new MessageDelegate(mw));
		mChannelPages->addWidget(mw);
		connect(mw, &MessageListView::CurrentPageChanged, mMessageHeader, &MessageHeaderWidget::SetCurrentPage);
	}
}
void SlackLogViewer::LoadGroupMessages()
{
	QJsonDocument dms = LoadJsonFile(gSettings->value("History/LastLogFilePath").toString(), "mpims.json");
	if (dms.isNull()) return;
	const QJsonArray& arr = dms.array();
	if (arr.size() == 0) return;
	ChannelTreeModel* model = static_cast<ChannelTreeModel*>(mChannelView->model());
	QModelIndex parent = model->index((int)Channel::GROUP_MESSAGE, 0, QModelIndex());
	if (arr.size() > 0) model->insertRows(0, arr.size(), parent);

	int row = 0;
	for (auto gm : arr)
	{
		FindKeyAs find_as("mpims.json", row);
		auto o = gm.toObject();
		const QString& id = find_as.string(o, "id");
		const QString& name = find_as.string(o, "name");
		const QJsonArray& arr_ = find_as.array(o, "members");
		if (arr_.size() < 2) throw FatalError("The size of array \"members\" in \"mpims.json\" is less than 2.");
		QVector<QString> members(arr_.size());
		for (int i = 0; i < members.size(); ++i)
		{
			if (!arr_[i].isString()) throw FatalError("Non-string value exists in the array \"members\" of \"mpims.json\".");
			members[i] = arr_[i].toString();
		}
		model->SetGMUserInfo(row, id, name, members);
		++row;
	}

	for (int i = 0; i < arr.size(); ++i)
	{
		MessageListView* mw = new MessageListView();
		mw->setModel(new MessageListModel(mw));
		mw->setItemDelegate(new MessageDelegate(mw));
		mChannelPages->addWidget(mw);
		connect(mw, &MessageListView::CurrentPageChanged, mMessageHeader, &MessageHeaderWidget::SetCurrentPage);
	}
}
void SlackLogViewer::UpdateRecentFiles()
{
	QStringList ws = gSettings->value("History/LogFilePaths").toStringList();
	int s = gSettings->value("History/NumOfRecentLogFilePaths").toInt();
#if QT_VERSION_MAJOR==5
	int x = std::min(s, ws.size());
#elif QT_VERSION_MAJOR==6
	int x = std::min((qsizetype)s, ws.size());
#endif
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
void SlackLogViewer::OpenLogFileFolder()
{
	QString path = gSettings->value("History/LastLogFilePath").toString();
	path = QFileDialog::getExistingDirectory(this, "Open", path);

	OpenLogFile(path);
}
void SlackLogViewer::OpenLogFileZip()
{
	QString path = gSettings->value("History/LastLogFilePath").toString();
	path = QFileDialog::getOpenFileName(this, "Open", path, "Zip files (*.zip)");

	OpenLogFile(path);
}
void SlackLogViewer::OpenLogFile(const QString& path)
{
	if (path.isEmpty()) return;
	QFileInfo info(path);
	if (info.isDir())
	{
		QFile ch(path + "/channels.json");
		if (!ch.exists())
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("channels.json does not exist.");
			return;
		}
		QFile usr(path + "/users.json");
		if (!usr.exists())
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

	if (info.isDir())
	{
		QDir wsdir = path;
		gWorkspace = wsdir.dirName();
	}
	else
	{
		QFileInfo info(path);
		gWorkspace = info.baseName();
	}

	//cache用フォルダの作成
	{
		QDir dir;
		if ((!dir.exists(gCacheDir + gWorkspace) && !dir.mkdir(gCacheDir + gWorkspace)) ||
			(!dir.exists(CachePath(CacheType::TEXT)) && !dir.mkdir(CachePath(CacheType::TEXT))) ||
			(!dir.exists(CachePath(CacheType::IMAGE)) && !dir.mkdir(CachePath(CacheType::IMAGE))) ||
			(!dir.exists(CachePath(CacheType::PDF)) && !dir.mkdir(CachePath(CacheType::PDF))) ||
			(!dir.exists(CachePath(CacheType::OTHERS)) && !dir.mkdir(CachePath(CacheType::OTHERS))) ||
			(!dir.exists(IconPath()) && !dir.mkdir(IconPath())))
		{
			QErrorMessage* m = new QErrorMessage(this);
			m->setAttribute(Qt::WA_DeleteOnClose);
			m->showMessage("cannot make cache folder at " + gCacheDir);
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

	try
	{
		ClearUsersAndChannels();
		LoadUsers();
		LoadChannels();
		LoadDirectMessages();
		LoadGroupMessages();
		UpdateRecentFiles();
		if (!gChannelVector.empty()) SetChannel(Channel::CHANNEL, 0);
	}
	catch (const FatalError& e)
	{
		QErrorMessage* m = new QErrorMessage(this);
		m->setAttribute(Qt::WA_DeleteOnClose);
		m->showMessage(e.error());
		ClearUsersAndChannels();
	}
}
void SlackLogViewer::OpenOption()
{

}
void SlackLogViewer::Exit()
{
	QApplication::quit();
}
void SlackLogViewer::SetChannel(Channel::Type type, int index)
{
	int row = IndexToRow(type, index);
	ReplyListModel* tmodel = static_cast<ReplyListModel*>(mThread->model());
	if (!tmodel->IsEmpty() && tmodel->GetChannelIndex() != std::make_pair(type, index)) tmodel->Close();
	MessageListView* m = static_cast<MessageListView*>(mChannelPages->widget(row));
	const Channel& ch = [type, index]()
	{
		if (type == Channel::CHANNEL) return gChannelVector[index];
		else if (type == Channel::DIRECT_MESSAGE) return gDMUserVector[index];
		else if (type == Channel::GROUP_MESSAGE) return gGMUserVector[index];
		else throw FatalError("invalid channel type");
	}();
	//setCurrentIndexはCreateより先に呼ばなければならない。
	//でないと、MessagePagesにwidthがフィットされない状態でdelegateのsizeHintが呼ばれてしまうらしい。
	mChannelPages->setCurrentIndex(row);
	mChannelView->setCurrentIndex(mChannelView->model()->index(index, 0, mChannelView->model()->index((int)type, 0)));
	mStack->setCurrentWidget(mChannelPages);
	if (!m->IsConstructed())
	{
		bool res = m->Construct(type, index);
		if (!res)
		{
			//falseが返ってくるということは、何かしら読み込みに失敗している。
			return;
		}
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
void SlackLogViewer::SetChannelAndIndex(Channel::Type type, int ch, int messagerow, int parentrow)
{
	SetChannel(type, ch);
	int row = IndexToRow(type, ch);
	MessageListView* m = static_cast<MessageListView*>(mChannelPages->widget(row));
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
		//rowは+2する。index==0は親メッセージ、index==1はボーターなので。
		mThread->ScrollToRow(messagerow + 2);
	}
}
void SlackLogViewer::JumpToPage(int page)
{
	int row = mChannelView->currentIndex().row();
	if (row == -1) return;
	MessageListView* m = static_cast<MessageListView*>(mChannelPages->widget(row));
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
	const ChannelTreeModel* model = static_cast<const ChannelTreeModel*>(mChannelView->model());
	QModelIndex index = mChannelView->currentIndex();
	auto [ch_type, ch_index] = model->GetChannelIndex(index);
	if (ch_index < 0 && mode.GetRangeMode() == SearchMode::CURRENTCH) return;

	auto [n, b] = mSearchView->Search(ch_type, ch_index, mChannelPages, key, mode);
	mStack->setCurrentWidget(mSearchView);
	if (n != 0)
	{
		//n == 0、つまり要素が一つも見つからなかった場合は、Openを呼んではいけない。
		dynamic_cast<SearchResultModel*>(mSearchView->GetView()->model())->Open(&mSearchView->GetMessages());
		if (b) mSearchView->GetView()->scrollTo(mSearchView->GetView()->model()->index(0, 0));
	}
}
void SlackLogViewer::CacheAllFiles(CacheStatus::Channel ch, CacheType type)
{
	//検索範囲
	std::vector<std::pair<Channel::Type, int>> chs;
	if (ch == CacheStatus::ALLCHS)
	{
		chs.resize(gChannelVector.size() + gDMUserVector.size() + gGMUserVector.size());
		int i = 0;
		int j = 0;
		for (auto& c : gChannelVector) { chs[i] = { Channel::CHANNEL, j }; ++i, ++j; }
		j = 0;
		for (auto& c : gDMUserVector) { chs[i] = { Channel::DIRECT_MESSAGE, j }; ++i, ++j; }
		j = 0;
		for (auto& c : gGMUserVector) { chs[i] = { Channel::GROUP_MESSAGE, j }; ++i, ++j; }
	}
	else if (ch == CacheStatus::CURRENTCH)
	{
		auto [ch_type, ch_index] = RowToIndex(mChannelView->currentIndex().row());
		chs = { { ch_type, ch_index } };
	}
	QList<QFuture<CacheResult>> fs;
	for (auto [ch_type, ch_index] : chs)
	{
		int row = IndexToRow(ch_type, ch_index);
		MessageListView* mes = static_cast<MessageListView*>(mChannelPages->widget(row));
		fs.append(QtConcurrent::run(::CacheAllFiles, ch_type, ch_index, mes, type));
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
void SlackLogViewer::ClearCache(CacheType type)
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
	d->setFixedWidth(640);
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
			"Qt Copyright (C) 2020 The Qt Company Ltd. distributed under LGPL License<br/>"
			"emojicpp Copyright (c) 2018 Shalitha Suranga distributed under MIT License<br/>"
			"CMakeHelpers Copyright (C) 2015 halex2005 distributed under MIT License<br/>"
			"QuaZip Copyright (C) 2005-2020 Sergey A. Tachenov and contributors distributed under LGPL License<br/>"
			"zlib Copyright (C) 1995-2017 Jean-loup Gailly and Mark Adler distributed under zlib License<br/>");
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

ChannelTreeModel::ChannelTreeModel()
	: mPublicIcon(ResourcePath("hash.svg")), mPrivateIcon(ResourcePath("lock.svg"))
{
}
QVariant ChannelTreeModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid()) return QVariant();
	if (role == Qt::DisplayRole)
	{
		const Channel* ch = static_cast<const Channel*>(index.internalPointer());
		if (ch->IsParent())
		{
			if (ch->GetType() == Channel::CHANNEL) return QString("Channels");
			if (ch->GetType() == Channel::DIRECT_MESSAGE) return QString("Direct messages");
			if (ch->GetType() == Channel::GROUP_MESSAGE) return QString("Group messages");
		}
		return ch->GetName();
	}
	else if (role == Qt::DecorationRole)
	{
		const Channel* ch = static_cast<const Channel*>(index.internalPointer());
		if (ch->IsParent()) return QVariant();
		if (ch->GetType() == Channel::CHANNEL)
		{
			if (!ch->IsPrivate()) return mPublicIcon;
			else return mPrivateIcon;
		}
		else if (ch->GetType() == Channel::DIRECT_MESSAGE)
		{
			auto& member = GetChannel(Channel::DIRECT_MESSAGE, index.row()).GetMembers().first();
			auto uit = gUsers.find(member);
			if (uit == gUsers.end()) throw FatalError(("user id:\"" + member +"\" not found").toLocal8Bit());
			return uit->GetIcon();
		}
		else if (ch->GetType() == Channel::GROUP_MESSAGE)
		{

		}
	}
	return QVariant();
}
int ChannelTreeModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid()) return (int)Channel::END_CH;
	if (parent.parent().isValid()) return 0;
	if (parent.row() == (int)Channel::CHANNEL) return (int)gChannelVector.size();
	if (parent.row() == (int)Channel::DIRECT_MESSAGE) return (int)gDMUserVector.size();
	if (parent.row() == (int)Channel::GROUP_MESSAGE) return (int)gGMUserVector.size();
	throw FatalError("unknown channel type");
}
int ChannelTreeModel::columnCount(const QModelIndex& parent) const
{
	return 1;
}
QModelIndex ChannelTreeModel::index(int row, int /*column*/, const QModelIndex& parent) const
{
	void* ptr = nullptr;
	if (!parent.isValid())
	{
		//parentがvalidでないなら、public, private, dmを抱える親ノードである。
		if (row >= (int)Channel::END_CH) return QModelIndex();
		ptr = &gChannelParentVector[row];
	}
	else
	{
		//parentがvalidなら、public_ch、private_ch、dmいずれかである。
		if (parent.row() == (int)Channel::CHANNEL && row < gChannelVector.size()) ptr = &gChannelVector[row];
		else if (parent.row() == (int)Channel::DIRECT_MESSAGE && row < gDMUserVector.size()) ptr = &gDMUserVector[row];
		else if (parent.row() == (int)Channel::GROUP_MESSAGE && row < gGMUserVector.size()) ptr = &gGMUserVector[row];
		else return QModelIndex();
	}
	return createIndex(row, 0, ptr);
}
QModelIndex ChannelTreeModel::parent(const QModelIndex& index) const
{
	const Channel* ch = static_cast<const Channel*>(index.internalPointer());
	if (!index.isValid()) return QModelIndex();
	if (ch->IsParent()) return QModelIndex();
	int type = ch->GetType();
	return createIndex(type, 0, &gChannelParentVector[type]);
}
bool ChannelTreeModel::insertRows(int row, int count, const QModelIndex& parent)
{
	//parentが最上位ノードの場合、insertは不可能。
	if (!parent.isValid()) return false;
	if (parent.row() >= (int)Channel::END_CH) return false;
	beginInsertRows(parent, row, row + count - 1);
	if (parent.row() == (int)Channel::CHANNEL) gChannelVector.insert(row, count, Channel());
	else if (parent.row() == (int)Channel::DIRECT_MESSAGE) gDMUserVector.insert(row, count, Channel());
	else if (parent.row() == (int)Channel::GROUP_MESSAGE) gGMUserVector.insert(row, count, Channel());
	endInsertRows();
	return true;
}
bool ChannelTreeModel::removeRows(int row, int count, const QModelIndex& parent)
{
	//parentが最上位ノードの場合、removeは不可能。
	if (!parent.isValid()) return false;
	if (parent.row() >= (int)Channel::END_CH) return false;
	beginRemoveRows(parent, row, row + count - 1);
	if (parent.row() == (int)Channel::CHANNEL) gChannelVector.remove(row, count);
	else if (parent.row() == (int)Channel::DIRECT_MESSAGE) gDMUserVector.remove(row, count);
	else if (parent.row() == (int)Channel::GROUP_MESSAGE) gGMUserVector.remove(row, count);
	endInsertRows();
	return true;
}

void ChannelTreeModel::SetChannelInfo(int row, bool is_private, const QString& id, const QString& name, const QVector<QString>& members)
{
	gChannelVector[row].SetChannelInfo(Channel::CHANNEL, is_private, id, name, members);
}
void ChannelTreeModel::SetDMUserInfo(int row, const QString& id, const QString& name, const QVector<QString>& members)
{
	gDMUserVector[row].SetChannelInfo(Channel::DIRECT_MESSAGE, id, name, members);
}
void ChannelTreeModel::SetGMUserInfo(int row, const QString& id, const QString& name, const QVector<QString>& members)
{
	gGMUserVector[row].SetChannelInfo(Channel::GROUP_MESSAGE, id, name, members);
}
std::pair<Channel::Type, int> ChannelTreeModel::GetChannelIndex(const QModelIndex& index) const
{
	if (!index.isValid()) return { Channel::END_CH, -1 };
	const Channel* ch = static_cast<const Channel*>(index.internalPointer());
	QModelIndex p = parent(index);
	if (!p.isValid()) return { ch->GetType(), -1 };
	return { ch->GetType(), index.row() };
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
	if (npages <= currentpage) throw FatalError("invalid page number");
	mNumOfPages = npages;
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
void MessageHeaderWidget::JumpToTop() { Q_EMIT JumpRequested(0); }
void MessageHeaderWidget::JumpToPrev() { Q_EMIT JumpRequested(mCurrentPage - 1); }
void MessageHeaderWidget::JumpToNext() { Q_EMIT JumpRequested(mCurrentPage + 1); }
void MessageHeaderWidget::JumpToBottom() { Q_EMIT JumpRequested(mNumOfPages - 1); }
void MessageHeaderWidget::JumpTo(int index)
{
	//通常はmsNumOfButtons/2が現在のページを示すボタンのはずだが、
	//ページの端ではそうならないので、分岐する。
	if (mCurrentPage < msNumOfButtons / 2)
	{
		//msNumOfButtons/2==2とすると、
		//0～1ページにいる場合、
		//indexは単純にページ番号を表す。
		Q_EMIT JumpRequested(index);
	}
	else if (mNumOfPages - mCurrentPage - 1 < msNumOfButtons / 2)
	{
		//ページの最後の方にいる場合。
		Q_EMIT JumpRequested(mNumOfPages - (msNumOfButtons - index));
	}
	else
	{
		//ページの両端以外にいる場合、
		Q_EMIT JumpRequested(mCurrentPage + index - msNumOfButtons / 2);
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
