#include <filesystem>
#include "MessageListView.h"
#include "GlobalVariables.h"
#include "SlackLogViewer.h"
#include "AttachedFile.h"
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScrollBar>
#include <QMouseEvent>
#include <QPushButton>
#include <QFileDialog>
#include <QErrorMessage>
#include <QGridLayout>
#include <QPainterPath>
#include <QtConcurrent>
#include <QMessageBox>
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>
#include "emoji.h"

//添付ファイルのダウンロードボタンを押したときに呼ばれる処理。
//1. ダイアログから保存先とファイル名を指定させる。
//2. ファイルのダウンロードを要求。
//3a. 既にダウンロードできているorダウンロードに成功した場合は指定先に保存する。
//3b. ダウンロードに失敗した場合はエラーを表示する。
void DownloadCacheFile(const AttachedFile* file)
{
	QString path = QFileDialog::getSaveFileName(MainWindow::Get(),
												"download",
												gSettings->value("History/LastLogFilePath").toString() + "/" + file->GetFileName());
	if (path.isEmpty()) return;

	auto save = [path](const AttachedFile* a)
	{
		QFile f(a->GetCacheFilePath());
		if (!f.exists()) throw FatalError("cached file cannot open");
		f.copy(path);
	};
	auto fail = [](const AttachedFile*)
	{
		QErrorMessage* m = new QErrorMessage(MainWindow::Get());
		m->showMessage("Download failed.");
	};
	file->Download(save, nullptr, save, fail);
	file->Wait();
}

ImageWidget::ImageWidget(const ImageFile* image, int pwidth, bool isquote)
	: mImage(image)
{
	QSize size;
	bool isnull = !image->IsLoaded();
	if (isnull)
	{
		QPixmap p = QPixmap::fromImage(gTempImage->scaledToHeight(gMaxThumbnailHeight, Qt::SmoothTransformation));
		size = p.size();
		setPixmap(p);
	}
	else
	{
		QImage p = image->GetImage().scaledToHeight(gMaxThumbnailHeight, Qt::SmoothTransformation);
		size = p.size();
		setPixmap(QPixmap::fromImage(p));
	}
	setStyleSheet("border: 2px solid rgb(160, 160, 160);");
	int imagewidth = std::min(pwidth - gSpacing * 5 - gIconSize, size.width());
	setFixedSize(imagewidth + 4, size.height() + 4);
	if (!isnull && !isquote)
	{
		//画像がダウンロード済みならダウンロードボタンを表示する。
		//ただし、引用ページの画像である場合は行わない。
		QPushButton* download = new QPushButton(this);
		download->setStyleSheet("border: 1px solid rgb(160, 160, 160); background-color: white;");
		download->setIcon(QIcon(ResourcePath("download.png")));
		const int width = 32;
		const int height = 32;
		download->setFixedWidth(width);
		download->setFixedHeight(height);
		QRect rect = this->rect();
		download->move(rect.right() - width - gSpacing, rect.top() + gSpacing);
		connect(download, &QPushButton::clicked, [image]()
				{
					DownloadCacheFile(image);
				});
	}
	else
	{
		setCursor(Qt::PointingHandCursor);
	}
}

DocumentWidget::DocumentWidget(const AttachedFile* file, int /*pwidth*/)
	: mFile(file)
{
	QGridLayout* layout = new QGridLayout();
	layout->setContentsMargins(gSpacing, gSpacing, gSpacing, gSpacing);
	QLabel* icon = new QLabel();
	icon->setStyleSheet("border: 0px; ");
	icon->setPixmap(QPixmap::fromImage(gDocIcon->scaledToHeight(gIconSize, Qt::SmoothTransformation)));
	icon->setFixedSize(gIconSize, gIconSize);
	layout->addWidget(icon, 0, 0, 2, 1);

	QFont font;
	font.setPointSizeF(GetDateTimePointSize());
	QLabel* name = new QLabel();
	name->setText(file->GetFileName());
	name->setFont(font);
	name->setStyleSheet("border: 0px; ");
	layout->addWidget(name, 0, 1);
	QLabel* size = new QLabel();
	size->setStyleSheet("border: 0px; ");
	size->setFont(font);

	int s = file->GetFileSize();
	int kmg = 0;
	while (s >= 1024)
	{
		kmg += 1;
		s /= 1024;
		if (kmg >= 4) break;
	}
	switch (kmg)
	{
	case 0: size->setText(QString::number(s) + " B"); break;
	case 1: size->setText(QString::number(s) + " KB"); break;
	case 2: size->setText(QString::number(s) + " MB"); break;
	case 3: size->setText(QString::number(s) + " GB"); break;
	case 4: size->setText(QString::number(s) + " TB"); break;
	}
	layout->addWidget(size, 1, 1);
	layout->setColumnStretch(2, 0);
	setLayout(layout);
	setStyleSheet("background-color: rgb(255, 255, 255); border: 2px solid rgb(160, 160, 160);");
	setFixedSize(gMaxThumbnailWidth + 4, gIconSize + gSpacing * 2 + 4);
	setCursor(Qt::PointingHandCursor);

	QPushButton* download = new QPushButton(this);
	download->setStyleSheet("border: 1px solid rgb(160, 160, 160); background-color: white;");
	download->setIcon(QIcon(ResourcePath("download.png")));
	const int width = 32;
	const int height = 32;
	download->setFixedWidth(width);
	download->setFixedHeight(height);
	QRect rect = this->rect();
	download->move(rect.right() - width - gSpacing, rect.top() + gSpacing);
	connect(download, &QPushButton::clicked, [file]()
			{
				DownloadCacheFile(file);
			});
}
void DocumentWidget::mousePressEvent(QMouseEvent*)
{
	Q_EMIT clicked(mFile);
}

ThreadWidget::ThreadWidget(Message& m, QSize threadsize)
	: mParentMessage(&m)
{
	setFixedHeight(threadsize.height());
	setFixedWidth(512);
	QHBoxLayout* l = new QHBoxLayout();
	l->setContentsMargins(gSpacing - 1, gSpacing - 1, gSpacing - 1, gSpacing - 1);
	l->setSpacing(gSpacing);
	const Thread* th = m.GetThread();
	for (auto& u : th->GetReplyUsers())
	{
		QLabel* icon = new QLabel();
		icon->setStyleSheet("border: 0px;");
		auto it = gUsers.find(u);
		if (it == gUsers.end())
			icon->setPixmap(gEmptyUser->GetPixmap().scaled(gThreadIconSize, gThreadIconSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		else icon->setPixmap(it.value().GetPixmap().scaled(gThreadIconSize, gThreadIconSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		l->addWidget(icon);
	}
	QLabel* rep = new QLabel();
	rep->setStyleSheet("color: #195BB2; border: 0px;");
	QFont f;
	f.setPointSizeF(GetThreadPointSize());
	f.setBold(true);
	rep->setFont(f);
	rep->setText(QString::number(th->GetReplies().size()) + " replies");
	l->addWidget(rep);
	mViewMessage = new QLabel();
	mViewMessage->setStyleSheet("color: rgb(64, 64, 64); border: 0px");
	f.setBold(false);
	mViewMessage->setFont(f);
	mViewMessage->setText(" View thread");
	mViewMessage->setVisible(false);
	l->addWidget(mViewMessage);
	l->addStretch();
	setLayout(l);
	setStyleSheet("border: 1px solid rgb(239, 239, 239);");
	setCursor(Qt::PointingHandCursor);
}

void ThreadWidget::mousePressEvent(QMouseEvent*)
{
	Q_EMIT clicked(mParentMessage);
}
#if QT_VERSION_MAJOR==5
void ThreadWidget::enterEvent(QEvent*)
#elif QT_VERSION_MAJOR==6
void ThreadWidget::enterEvent(QEnterEvent*)
#endif
{
	setStyleSheet("background-color: rgb(255, 255, 255); border: 1px solid rgb(192, 192, 192); border-radius: 5px;");
	mViewMessage->setVisible(true);
}
void ThreadWidget::leaveEvent(QEvent*)
{
	setStyleSheet("border: 1px solid rgb(239, 239, 239);");
	mViewMessage->setVisible(false);
}

MessageListView::MessageListView()
	: mPreviousPage(-1), mConstructed(false)
{
	setMouseTracking(true);
	setVerticalScrollMode(ScrollPerPixel);
	verticalScrollBar()->setSingleStep(gSettings->value("ScrollStep").toInt());
	setStyleSheet("border: 0px;");
	auto* scroll = verticalScrollBar();
	connect(scroll, SIGNAL(valueChanged(int)), this, SLOT(UpdateCurrentPage()));
}
bool MessageListView::Construct(Channel::Type type, int index)
{
	QString folder_or_zip = gSettings->value("History/LastLogFilePath").toString();
	QFileInfo info(folder_or_zip);
	using MessagesAndReplies = std::pair<std::vector<std::shared_ptr<Message>>, std::vector<std::shared_ptr<Message>>>;
	std::map<QDateTime, QFuture<MessagesAndReplies>> messages_and_replies;
	std::mutex thmtx;
	const QString& dirname = GetChannel(type, index).GetDirName();

	if (info.isDir())
	{
		//ディレクトリの場合。
		QDir dir = folder_or_zip + "/" + dirname;
		QStringList ext = { "*.json" };//jsonファイルだけ読む。そもそもjson以外存在しないけど。
		QStringList filenames = dir.entryList(ext, QDir::Files, QDir::Name);
		for (auto& name : filenames)
		{
			int end = name.lastIndexOf('.');
			auto dtstr = name.left(end);
			QDateTime d = QDateTime::fromString(dtstr, Qt::ISODate);
			QString filename = folder_or_zip + "/" + dirname + "/" + name;
			QFile file(filename);
			if (!file.open(QIODevice::ReadOnly)) exit(-1);
			QByteArray dat = file.readAll();
			messages_and_replies.insert(std::make_pair(std::move(d),
													   QtConcurrent::run(&Construct_parallel,
																	filename,
																	std::make_pair(type, index), std::move(dat),
																	&mThreads, &thmtx)));
		}
	}
	else
	{
		if (info.suffix() != "zip") exit(-1);
		QuaZip zip(folder_or_zip);
		zip.open(QuaZip::mdUnzip);
		zip.setFileNameCodec("UTF-8");
		QuaZipFileInfo64 info64;
		for (bool b = zip.goToFirstFile(); b; b = zip.goToNextFile())
		{
			zip.getCurrentFileInfo(&info64);
			if (!info64.name.endsWith(".json")) continue;
			//ここはQDirではなくstd::filesystem::pathを使う。
			//というのも、QDirは存在しないディレクトリを扱えないため。
			//今、dirはzipファイル内の構造を示しており、つまり実在しないディレクトリである。
			//したがって、std::filesystem::pathでないと扱うことが出来ない。
			std::filesystem::path dir = (const char*)info64.name.toLocal8Bit();
			auto qstr = QString::fromStdString(dir.parent_path().u8string());
			if (qstr != dirname) continue;
			auto jsonname = dir.stem();
			QDateTime d = QDateTime::fromString(QString::fromLocal8Bit(jsonname.string().c_str()), Qt::ISODate);
			QString dtstr = d.toString();
			QuaZipFile file(&zip);
			file.open(QIODevice::ReadOnly);
			QByteArray dat = file.readAll();
			messages_and_replies.insert(std::make_pair(std::move(d),
													   QtConcurrent::run(&Construct_parallel,
																	info64.name,
																	std::make_pair(type, index), std::move(dat),
																	&mThreads, &thmtx)));
		}
	}

	//スレッドごとに得た結果を統合する。
	QDate prev;
	int row_count = 0;
	FILE* errlog = nullptr;
	auto open = []()
	{
		QDir exedir = QCoreApplication::applicationDirPath();
		QString settingdir = exedir.absolutePath() + "/errorlog.txt";
		return fopen(settingdir.toLocal8Bit(), "w");
	};

	for (auto& [dt, fut] : messages_and_replies)
	{
		//fut.waitForFinished();
		try
		{
			auto [meses, reps] = fut.result();
			//mMessages.insert(mMessages.end(), std::make_move_iterator(meses.begin()), std::make_move_iterator(meses.end()));
			for (auto& mes : meses)
			{
				if (*gDateSeparator && (prev.isNull() || mes->GetTimeStamp().date() > prev))
				{
					//日付が変わった場合、セパレータを挿入する。
					prev = mes->GetTimeStamp().date();
					auto m = std::make_shared<Message>(type, index, mes->GetTimeStamp());
					m->SetRow(row_count);
					++row_count;
					mMessages.emplace_back(std::move(m));
				}
				mes->SetRow(row_count);
				mMessages.emplace_back(std::move(mes));
				++row_count;
			}
			for (auto& rep : reps)
			{
				//リプライはthreadsの方に格納していく必要がある。
				auto it = mThreads.find(rep->GetThreadTimeStampStr());
				if (it == mThreads.end())
				{
					//何故か親メッセージが見つからないリプライがたまにある。
					//以前は通常のメッセージとして表示していたが、もうめんどいから無視してしまおう。
					continue;
				}
				rep->SetThread(it->second.get());
				it->second->AddReply(rep);
			}
		}
		catch (FatalError& e)
		{
			if (errlog == nullptr) errlog = open();
			fprintf(errlog, "%s\n", e.error().toLocal8Bit().data());
		};
	}
	if (errlog != nullptr)
	{
		fclose(errlog);
		QMessageBox q;
		q.setText(QString::fromStdString("Error : Loading of the export files was interrupted due to some formatting errors. See errorlog.txt for details."));
		q.exec();
		Close();
		return false;
	}

	mConstructed = true;
	static_cast<MessageListModel*>(model())->Open(&mMessages);
	UpdateCurrentPage();
	return true;
}
std::pair<std::vector<std::shared_ptr<Message>>, std::vector<std::shared_ptr<Message>>>
MessageListView::Construct_parallel(const QString& filename, std::pair<Channel::Type, int> ch_type_index, QByteArray data,
									std::map<QString, std::shared_ptr<Thread>>* threads, std::mutex* thmtx)
{
	auto [type, ch_index] = ch_type_index;
	std::vector<std::shared_ptr<Message>> messages;
	std::vector<std::shared_ptr<Message>> replies;
	const QJsonArray a = QJsonDocument::fromJson(data).array();

	for (auto v = a.begin(); v != a.end(); ++v)
	{
		qsizetype index = v - a.begin();

		auto get_as = FindKeyAs(filename, index);
		auto value_to = ValueTo(filename, index);

		auto a = *v;//Qt5とQt6で戻り値の型が異なるので、autoで。
		QJsonObject o = a.toObject();
		{
			auto hidden = o.find("hidden");
			if (hidden != o.end() && hidden.value().toBool() == true) continue;
			auto hidden_by_limit = o.find("is_hidden_by_limit");
			if (hidden_by_limit != o.end()) continue;
		}
		auto it = o.find("thread_ts");
		std::shared_ptr<Message> mes;
		if (it != o.end())
		{
			const QString& thread_ts = value_to.string(it);
			//スレッドの親メッセージかリプライは、thread_tsを持つ。
			auto it2 = o.find("replies");
			if (it2 != o.end())
			{
				//"replies"を持つので親メッセージ。
				//const QJsonArray& ra = it2.value().toArray();
				const QJsonArray& ausers = get_as.array(o, "reply_users");
				std::vector<QString> users(ausers.size());
				for (int i = 0; i < ausers.size(); ++i) users[i] = ausers[i].toString();
				mes = std::make_shared<Message>(filename, index, type, ch_index, o, thread_ts);
				messages.emplace_back(mes);
				std::lock_guard<std::mutex> lg(*thmtx);
				auto [thit, b] = threads->insert(std::make_pair(thread_ts, std::make_shared<Thread>(mes.get(), std::move(users))));
				mes->SetThread(thit->second.get());
			}
			else
			{
				//"replies"を持たないのでリプライである。
				/*const QString& key = it.value().toString();
				auto thit = mThreads.find(key);
				if (thit == mThreads.end())
				{
					//リプライよりも親メッセージが先に見つかっているはずなので、
					//理屈の上ではここでmThreadsから見つかってこないはずがない。
					//のだが、たまに親メッセージのリプライ情報が欠損していることがあるようで、その場合は通常のメッセージとして表示する。
					messages.emplace_back(std::make_shared<Message>(ch_index, o));
				}
				else thit->second.AddReply(ch_index, o);*/
				mes = std::make_shared<Message>(filename, index, type, ch_index, o, std::move(thread_ts));
				replies.emplace_back(mes);
			}
		}
		else
		{
			mes = std::make_shared<Message>(filename, index, type, ch_index, o);
			messages.emplace_back(mes);
		}
	}
	return { std::move(messages), std::move(replies) };
}
void MessageListView::Clear()
{
	for (auto& m : mMessages)
	{
		m->ClearTextDocument();
		{
			auto& afs = m->GetFiles();
			if (!afs.empty())
			{
				for (auto& f : afs)
				{
					if (!f->IsImage()) continue;
					ImageFile* i = static_cast<ImageFile*>(f.get());
					if (!i->HasImage()) continue;
					i->ClearImage();
				}
			}
		}
		Thread* th = m->GetThread();
		if (!th) continue;
		for (auto& t : th->GetReplies())
		{
			t->ClearTextDocument();
			{
				auto& afs = t->GetFiles();
				if (!afs.empty())
				{
					for (auto& f : afs)
					{
						if (!f->IsImage()) continue;
						ImageFile* i = static_cast<ImageFile*>(f.get());
						if (!i->HasImage()) continue;
						i->ClearImage();
					}
				}
			}
		}
	}
}
void MessageListView::Close()
{
	static_cast<MessageListModel*>(model())->Close();
	mCurrentIndex = QModelIndex();
	mSelectedIndex = QModelIndex();
	mPreviousPage = -1;
	mConstructed = false;
	mMessages.clear();
	mThreads.clear();
}
void MessageListView::UpdateCurrentIndex(QPoint pos)
{
	QModelIndex index = indexAt(pos);
	if (index == mCurrentIndex) return;
	if (mCurrentIndex.isValid() && mCurrentIndex != mSelectedIndex)
		closePersistentEditor(mCurrentIndex);
	if (index.isValid() && index != mSelectedIndex)
		openPersistentEditor(index);
	mCurrentIndex = index;
}
void MessageListView::CloseEditorAtSelectedIndex(QPoint pos)
{
	QModelIndex index = indexAt(pos);
	if (mSelectedIndex.isValid()) closePersistentEditor(mSelectedIndex);
	if (mSelectedIndex == index) openPersistentEditor(index);
	mSelectedIndex = QModelIndex();
}
bool MessageListView::IsSelectedIndex(QPoint pos)
{
	QModelIndex index = indexAt(pos);
	return mSelectedIndex == index;
}
void MessageListView::mousePressEvent(QMouseEvent* event)
{
	QListView::mousePressEvent(event);
	CloseEditorAtSelectedIndex(event->pos());
}
void MessageListView::mouseMoveEvent(QMouseEvent* event)
{
	QListView::mouseMoveEvent(event);
	UpdateCurrentIndex(event->pos());
}
void MessageListView::wheelEvent(QWheelEvent* event)
{
	QListView::wheelEvent(event);
	UpdateCurrentIndex(event->position().toPoint());
}

void MessageListView::leaveEvent(QEvent* event)
{
	if (mCurrentIndex.isValid() && mCurrentIndex != mSelectedIndex)
		closePersistentEditor(mCurrentIndex);
	mCurrentIndex = QModelIndex();
	QListView::leaveEvent(event);
}
int MessageListView::GetCurrentRow()
{
	int x = this->rect().center().x();
	int y = this->rect().top();
	return indexAt(QPoint(x, y)).row();
}
int MessageListView::GetCurrentPage()
{
	int row = GetCurrentRow();
	int mpp = gSettings->value("NumOfMessagesPerPage").toInt();
	return row / mpp;
}

bool MessageListView::ScrollToRow(int row)
{
	if (row >= mMessages.size()) return false;
	auto* model = this->model();
	while (model->rowCount() <= row)
	{
		model->fetchMore(QModelIndex());
	}
	scrollTo(this->model()->index(row, 0, QModelIndex()), QAbstractItemView::PositionAtTop);
	return true;
}
bool MessageListView::JumpToPage(int page)
{
	int row = page * gSettings->value("NumOfMessagesPerPage").toInt();
	return ScrollToRow(row);
}
void MessageListView::UpdateCurrentPage()
{
	int page = GetCurrentPage();
	if (page != mPreviousPage)
	{
		mPreviousPage = page;
		Q_EMIT CurrentPageChanged(page);
	}
}
void MessageListView::UpdateSelection(bool b)
{
	if (!b)
	{
		//if (mCurrentIndex != mSelectedIndex)
		//	closePersistentEditor(mSelectedIndex);//これが呼ばれることはどうやらないらしい。振る舞いを把握しきれていないので一応書いておくが。
		mSelectedIndex = QModelIndex();
		//printf("deselected\n");
	}
	else
	{
		mSelectedIndex = mCurrentIndex;
	}
}


MessageListModel::MessageListModel(QListView* list)
	: mListView(list), mMessages(nullptr), mSize(0)
{
	connect(this, &MessageListModel::ImageDownloadFinished, this, &MessageListModel::dataChanged, Qt::QueuedConnection);
}
QVariant MessageListModel::data(const QModelIndex& index, int role) const
{
	if (role != Qt::DisplayRole) return QVariant();
	const Message* ch = static_cast<const Message*>(index.internalPointer());
	return ch->GetMessage();
}
int MessageListModel::rowCount(const QModelIndex& parent) const
{
	if (mMessages == nullptr) return 0;
	if (!parent.isValid()) return mSize;
	return 0;
}
QModelIndex MessageListModel::index(int row, int, const QModelIndex& parent) const
{
	//parentがrootnodeでない場合は子ノードは存在しない。
	if (parent.isValid()) return QModelIndex();
	if (mMessages == nullptr) return QModelIndex();
	if (row >= mSize) return QModelIndex();
	return createIndex(row, 0, (void*)(*mMessages)[row].get());
}
bool MessageListModel::insertRows(int row, int count, const QModelIndex& parent)
{
	beginInsertRows(parent, row, row + count - 1);
	mSize += count;
	endInsertRows();
	return true;
}
bool MessageListModel::removeRows(int row, int count, const QModelIndex& parent)
{
	beginRemoveRows(parent, row, row + count - 1);
	mSize -= count;
	endRemoveRows();
	return true;
}
bool MessageListModel::canFetchMore(const QModelIndex& parent) const
{
	if (parent.isValid()) return false;
	if (mMessages == nullptr) return false;
	return mSize < mMessages->size();
}
void MessageListModel::fetchMore(const QModelIndex& parent)
{
	if (parent.isValid()) return;
	int fetchsize = std::min(gSettings->value("NumOfMessagesPerPage").toInt(), (int)(mMessages->size() - mSize));
	insertRows(mSize, fetchsize);
}
int MessageListModel::GetTrueRowCount() const
{
	return (int)mMessages->size();
}

void MessageListModel::Open(const std::vector<std::shared_ptr<Message>>* m)
{
	mMessages = m;
	int fetchsize = std::min(gSettings->value("NumOfMessagesPerPage").toInt(), (int)(mMessages->size()));
	if (fetchsize > 0) insertRows(0, fetchsize);
}
void MessageListModel::Close()
{
	int c = rowCount();
	if (c > 0) removeRows(0, c);
	mMessages = nullptr;
}

void MessageBrowser::mousePressEvent(QMouseEvent* event)
{
	MessageEditor* e = static_cast<MessageEditor*>(parent());
	MessageListView* v = static_cast<MessageListView*>(e->GetMessageListView());
	QPoint pos = v->mapFromGlobal(event->globalPos());
	//もしクリックされたのが選択されているindexなら、処理は自分で行えばいい。
	//しかし自分自身でない場合、選択解除ができないのでCloseEditorを呼ぶ。
	if (!v->IsSelectedIndex(pos)) v->CloseEditorAtSelectedIndex(pos);
	QTextBrowser::mousePressEvent(event);
}

MessageEditor::MessageEditor(MessageListView* view, Message& m, QSize namesize, QSize datetimesize, QSize textsize, QSize threadsize, int pwidth, bool has_thread)
	: mListView(view)
{
	setStyleSheet("QWidget { background-color: rgb(239, 239, 239); border: 0px; }");
	setAutoFillBackground(true);
	setMouseTracking(true);
	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(gLeftMargin, gTopMargin, gRightMargin, gBottomMargin);
	layout->setSpacing(gSpacing);

	{
		//icon
		QPixmap icon(m.GetUser().GetPixmap().scaled(QSize(36, 36), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		ClickableLabel* label = new ClickableLabel();
		label->setStyleSheet("border: 0px;");
		label->setPixmap(icon);
		layout->addWidget(label);
		layout->setAlignment(label, Qt::AlignTop | Qt::AlignLeft);
		connect(label, &ClickableLabel::clicked, [u = &m.GetUser()]() { MainWindow::Get()->OpenUserProfile(u); });
	}
	QVBoxLayout* text = new QVBoxLayout();
	text->setContentsMargins(0, 0, 0, 0);
	text->setSpacing(gSpacing);
	layout->addLayout(text);
	QHBoxLayout* nmdt = new QHBoxLayout();
	nmdt->setContentsMargins(0, 0, 0, 0);
	nmdt->setSpacing(gSpacing);
	text->addLayout(nmdt);
	{
		//name
		const QString& n = m.GetUser().GetName();
		QLabel* name = nullptr;
		if (n.isEmpty()) name = new QLabel("noname");
		else name = new QLabel(n);
		name->setTextInteractionFlags(Qt::TextSelectableByMouse);
		name->setStyleSheet("border: 0px;");
		name->setFixedSize(namesize);
		QFont f;
		f.setPointSizeF(GetNamePointSize());
		f.setBold(true);
		name->setFont(f);
		nmdt->addWidget(name);
	}
	{
		//datetime
		QLabel* time = new QLabel(m.GetTimeStampStr());
		time->setTextInteractionFlags(Qt::TextSelectableByMouse);
		time->setStyleSheet("color: rgb(64, 64, 64); border: 0px;");
		time->setFixedHeight(datetimesize.height());
		QFont f;
		f.setPointSizeF(GetDateTimePointSize());
		time->setFont(f);
		nmdt->addWidget(time);
		//nmdt->setAlignment(time, Qt::AlignBottom | Qt::AlignLeft);
	}
	nmdt->addStretch();
	{
		//message
		MessageBrowser* tb = new MessageBrowser();
		//int height = tb->heightForWidth(tb->width());
		tb->setFixedSize(textsize);
		tb->setOpenExternalLinks(true);
		tb->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		tb->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		tb->setStyleSheet("QTextBrowser { border: 0px; }"
						  "QMenu::item:disabled { color: grey; }"
						  "QMenu::item:selected { background-color: palette(Highlight); }");
		connect(tb, SIGNAL(copyAvailable(bool)), this, SIGNAL(copyAvailable(bool)));
		QString html;
		//html += MrkdwnToHtml(m.GetMessage());
		//tb->setHtml(html);
		//tb->setHtml(m.GetTextDocument()->toHtml());
		tb->setDocument(m.GetTextDocument());
		text->addWidget(tb);
	}
	if (!m.GetQuotes().empty())
	{
		const auto& quotes = m.GetQuotes();
		for (auto& q : quotes)
		{
			if (q->HasImage())
			{
				ImageWidget* im = new ImageWidget(q->GetImageFile(), pwidth, true);
				text->addWidget(im);
				text->setAlignment(im, Qt::AlignLeft);
				//QObject::connect(im, &ImageWidget::clicked, [im]() { MainWindow::Get()->OpenImage(im->GetImage()); });
			}
			QHBoxLayout* l = new QHBoxLayout();
			//l->setSpacing(gSpacing);
			l->setContentsMargins(0, 0, 0, 0);
			l->addSpacing(gSpacing);

			MessageBrowser* tb = new MessageBrowser();
			int width = textsize.width();
			width = std::min(width, gMaxThumbnailWidth * 2);//テキスト全体の幅
			QTextDocument* td = q->GetTextDocument();
			td->setTextWidth(width - gSpacing * 2 - 2);//テキスト左側の縦線分のスペースを差し引く。
			int height = ceil(td->documentLayout()->documentSize().height());

			QFrame* vline = new QFrame();
			vline->setFrameShape(QFrame::VLine);
			vline->setLineWidth(1);
			vline->setFixedSize(2, height);
			vline->setStyleSheet("background-color: rgb(160, 160, 160);");
			l->addWidget(vline);
			l->setAlignment(vline, Qt::AlignLeft);

			tb->setFixedSize(width, height);
			tb->setOpenExternalLinks(true);
			tb->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			tb->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			tb->setStyleSheet("QTextBrowser { border: 0px; }"
							  "QMenu::item:disabled { color: grey; }"
							  "QMenu::item:selected { background-color: palette(Highlight); }");
			connect(tb, SIGNAL(copyAvailable(bool)), this, SIGNAL(copyAvailable(bool)));
			//QString html;
			//html += MrkdwnToHtml(m.GetMessage());
			//tb->setHtml(html);
			//tb->setHtml(m.GetTextDocument()->toHtml());
			tb->setDocument(q->GetTextDocument());
			l->addWidget(tb);
			l->setAlignment(tb, Qt::AlignLeft);
			l->addStretch();

			text->addLayout(l);
		}
	}
	if (!m.GetFiles().empty())
	{
		//Files
		const auto& files = m.GetFiles();
		for (auto& f : files)
		{
			if (f->IsImage())
			{
				ImageWidget* im = new ImageWidget(static_cast<const ImageFile*>(f.get()), pwidth);
				text->addWidget(im);
				text->setAlignment(im, Qt::AlignLeft);
				QObject::connect(im, &ImageWidget::clicked, [im]() { MainWindow::Get()->OpenImage(im->GetImage()); });
			}
			else
			{
				DocumentWidget* doc = new DocumentWidget(f.get(), pwidth);
				text->addWidget(doc);
				text->setAlignment(doc, Qt::AlignLeft);
				if (f->IsText())
				{
					QObject::connect(doc, &DocumentWidget::clicked, MainWindow::Get(), &SlackLogViewer::OpenText);
				}
				else if (f->IsPDF())
				{
					QObject::connect(doc, &DocumentWidget::clicked, MainWindow::Get(), &SlackLogViewer::OpenPDF);
				}
			}
		}

	}
	if (!m.GetReactions().empty())
	{
		//reactions
		QHBoxLayout* l = new QHBoxLayout();
		l->setSpacing(gSpacing);
		l->setContentsMargins(0, 0, 0, 0);
		auto& ra = m.GetReactions();
		for (auto& r : ra)
		{
			QString name = ":" + r.GetName() + ":";
			auto it = emojicpp::EMOJIS.find(name.toLocal8Bit().data());
			QString utficon;
			if (it != emojicpp::EMOJIS.end())
				utficon = (it->second.c_str());
			else utficon = name;
			QLabel* icon = new QLabel(utficon + " " + QString::number(r.GetUsers().size()));
			icon->setStyleSheet(
				"QLabel {"
				"border: 0px;"
				"background-color: rgb(224, 224, 224);"
				"border-radius: 10px;"
				"}"
				"QToolTip {"
				"background-color: black;"
				"color: white;"
				"}"
			);
			QFont f;
			f.setPointSizeF(GetReactionPointSize());
			icon->setFont(f);
			QString tooltip;
			const auto& users = r.GetUsers();
			auto finduser = [](const QString& id)
			{
				auto it = gUsers.find(id);
				if (it != gUsers.end()) return it.value().GetName();
				else return id;
			};
			tooltip += finduser(users[0]);
			for (int i = 1; i < users.size() - 1; ++i) tooltip += ", " + finduser(users[i]);
			if (users.size() > 1) tooltip += " and " + finduser(users.back());
			tooltip += " reacted with " + name;
			icon->setToolTip(tooltip);
			l->addWidget(icon);
		}
		l->addStretch();
		text->addLayout(l);
	}
	if (has_thread)
	{
		//thread
		ThreadWidget* w = new ThreadWidget(m, threadsize);
		text->addWidget(w);
		text->setAlignment(w, Qt::AlignLeft | Qt::AlignBottom);
		QObject::connect(w, &ThreadWidget::clicked, MainWindow::Get(), &SlackLogViewer::OpenThread);
	}
	text->addStretch();
	setLayout(layout);
}
#if QT_VERSION_MAJOR==5
void MessageEditor::enterEvent(QEvent*)
#elif QT_VERSION_MAJOR==6
void MessageEditor::enterEvent(QEnterEvent*)
#endif
{
	setStyleSheet("QWidget { background-color: rgb(239, 239, 239); }");
}
void MessageEditor::leaveEvent(QEvent*)
{
	setStyleSheet("QWidget { background-color: white; }");
}
void MessageEditor::mousePressEvent(QMouseEvent* evt)
{
	QPoint pos = mListView->mapFromGlobal(evt->globalPos());
	//もしクリックされたのが選択されているindexなら、処理は自分で行えばいい。
	//しかし自分自身でない場合、選択解除ができないのでCloseEditorを呼ぶ。
	if (!mListView->IsSelectedIndex(pos)) mListView->CloseEditorAtSelectedIndex(pos);
	//QWidgetのmousePressEventを読んでしまうと、多分イベントが消費されてMessageBrowserのクリック処理が呼ばれないんじゃないか。
	//右クリックの反応がなくなってしまう。
	//QWidget::mousePressEvent(event);
}
void MessageEditor::mouseMoveEvent(QMouseEvent* evt)
{
	//テキスト選択状態で生成済みのeditorに侵入したとき、
	//困ったことにMessageListViewのmouseMoveEventが呼ばれず、mCurrentIndexが更新されない。
	//なのでこちらから更新する。
	QPoint pos = mListView->mapFromGlobal(evt->globalPos());
	//もしクリックされたのが選択されているindexなら、処理は自分で行えばいい。
	//しかし自分自身でない場合、選択解除ができないのでCloseEditorを呼ぶ。
	mListView->UpdateCurrentIndex(pos);
	QWidget::mousePressEvent(evt);
}
void MessageEditor::paintEvent(QPaintEvent*)
{
	QStyleOption opt;
	opt.initFrom(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

MessageDelegate::MessageDelegate(MessageListView* list)
	: mListView(list)
{
}
QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt(option);
	initStyleOption(&opt, index);

	int width = opt.rect.width();

	if (Message* m = static_cast<Message*>(index.internalPointer()); m->IsSeparator())
	{
		return QSize(width, GetSeparatorSize(option, index).height());
	}
	QSize mssize = GetMessageSize(opt, index);
	int height = mssize.height();

	QSize qtsize = GetQuoteSize(opt, index);
	if (qtsize != QSize(0, 0)) height += qtsize.height() + gSpacing;

	QSize dcsize = GetDocumentSize(opt, index);
	if (dcsize != QSize(0, 0)) height += dcsize.height() + gSpacing;

	QSize rcsize = GetReactionSize(opt, index);
	if (rcsize != QSize(0, 0)) height += rcsize.height() + gSpacing;

	QSize thsize = GetThreadSize(opt, index);
	if (thsize != QSize(0, 0)) height += thsize.height() + gSpacing;

	height = height > gIconSize ? height : gIconSize;

	return QSize(width, gTopMargin + gBottomMargin + height);
}
void MessageDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt(option);
	initStyleOption(&opt, index);
	painter->save();
	const QRect& rect(option.rect);
	const QRect& crect(rect.adjusted(gLeftMargin, gTopMargin, -gRightMargin, -gBottomMargin));
	if (Message* m = static_cast<Message*>(index.internalPointer()); m->IsSeparator())
	{
		PaintSeparator(painter, rect, 0, opt, index);
		return;
	}
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
	painter->setRenderHint(QPainter::TextAntialiasing, true);
	int y = PaintMessage(painter, crect, 0, opt, index);
	y = PaintQuote(painter, crect, y, opt, index);
	y = PaintDocument(painter, crect, y, opt, index);
	y = PaintReaction(painter, crect, y, opt, index);
	y = PaintThread(painter, crect, y, opt, index);
	painter->restore();
}

QWidget* MessageDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	if (m->IsSeparator()) return nullptr;
	auto* w = new MessageEditor(mListView, *m,
								GetNameSize(option, index), GetDateTimeSize(option, index),
								GetTextSize(option, index), GetThreadSize(option, index), parent->width(), m->GetThread() != nullptr);
	connect(w, &MessageEditor::copyAvailable, mListView, &MessageListView::UpdateSelection);
	w->setParent(parent);
	return w;
}

int MessageDelegate::PaintMessage(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	const User* u = &m->GetUser();
	painter->save();

	crect.translate(0, ypos);

	// Draw message icon
	painter->drawPixmap(crect.left(), crect.top(),
						m->GetUser().GetPixmap().scaled(QSize(gIconSize, gIconSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

	// Draw name
	QSize nmsize = GetNameSize(option, index);
	QRect nmrect(crect.left() + gIconSize + gSpacing,
		crect.top(), nmsize.width(), nmsize.height());
	{
		QFont f(option.font);
		f.setPointSizeF(GetNamePointSize());
		f.setBold(true);
		painter->save();
		painter->setFont(f);
		const QString& name = u->GetName();
		painter->drawText(nmrect, Qt::TextSingleLine, name.isEmpty() ? "noname" : name);
		painter->restore();
	}

	// Draw datetime
	QSize dtsize = GetDateTimeSize(option, index);
	QRect dtrect(crect.left() + gIconSize + gSpacing + nmsize.width() + gSpacing,
		crect.top() + (nmsize.height() - dtsize.height()) / 2,
		dtsize.width(), dtsize.height());
	{
		QFont f(option.font);
		f.setPointSizeF(GetDateTimePointSize());
		painter->save();
		painter->setFont(f);
		painter->setPen(QPen(QBrush(QColor(64, 64, 64)), 2));
		painter->drawText(dtrect, Qt::TextSingleLine, m->GetTimeStampStr());
		painter->restore();
	}
	// Draw text
	QSize mssize = GetTextSize(option, index);
	QRect msrect(0, 0, mssize.width(), mssize.height());
	{
		painter->save();
		painter->setFont(option.font);
		painter->translate(QPoint(crect.left() + gIconSize + gSpacing, crect.top() + nmsize.height() + gSpacing));
		m->GetTextDocument()->drawContents(painter, msrect);
		painter->restore();
	}
	painter->restore();
	return ypos + nmsize.height() + gSpacing + mssize.height() + gSpacing;
}
int MessageDelegate::PaintQuote(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	const auto& qs = m->GetQuotes();
	if (qs.empty()) return ypos;
	crect.translate(0, ypos);
	int y = crect.top();
	auto* device = painter->device();

	int count = 0;
	for (auto& q : qs)
	{
		painter->save();
		if (q->HasImage())
		{
			//画像を持っている場合、それを表示する。

			auto draw = [](QPainter* painter, QRect crect, int y, const QImage& image)
			{
				QPixmap tmp = QPixmap::fromImage(image);
				painter->save();
				painter->setPen(QPen(QBrush(QColor(160, 160, 160)), 2));
				int width = std::min(crect.width() - gSpacing - gIconSize, tmp.width());
				if (width < tmp.width()) tmp = tmp.copy(0, 0, width, tmp.height());
				painter->drawPixmap(crect.left() + gIconSize + gSpacing + 2, y + 2, width, tmp.height(), tmp);
				painter->drawRect(crect.left() + gIconSize + gSpacing + 1, y + 1, width + 2, tmp.height() + 2);
				painter->restore();
			};

			auto* model = static_cast<MessageListModel*>(mListView->model());
			auto notify_fin_download = [model, index](const AttachedFile*)
			{
				Q_EMIT model->ImageDownloadFinished(index, index, QVector<int>());
			};
			auto draw_immediately = [draw, painter, crect, y](const AttachedFile* file)
			{
				const ImageFile* image = static_cast<const ImageFile*>(file);
				draw(painter, crect, y, image->GetImage().scaledToHeight(gMaxThumbnailHeight, Qt::SmoothTransformation));
			};
			auto draw_loading = [draw, painter, crect, y](const AttachedFile*)
			{
				draw(painter, crect, y, gTempImage->scaledToHeight(gMaxThumbnailHeight, Qt::SmoothTransformation));
			};
			auto draw_failed = [device, draw, crect, y](const AttachedFile*)
			{
				//これが呼ばれるタイミングでは、何故かpainterのポインタが不定値になっている。
				QPainter painter(device);
				draw(&painter, crect, y, gTempImage->scaledToHeight(gMaxThumbnailHeight, Qt::SmoothTransformation));
			};

			q->DownloadAndOpenImage(draw_immediately, draw_loading, notify_fin_download, draw_failed);

			y += gMaxThumbnailHeight + gSpacing + 4;
		}

		QSize textsize = GetQuoteTextSize(option, index, count);
		//引用文左の縦線分の幅を引く。
		QRect rect(0, 0, textsize.width() - gSpacing * 2 - 2, textsize.height());

		//引用文左側の縦線
		painter->save();
		painter->setPen(QPen(QBrush(QColor(160, 160, 160)), 2));
		painter->drawLine(crect.left() + gIconSize + gSpacing * 2 + 1, y, crect.left() + gIconSize + gSpacing * 2 + 1, y + textsize.height());
		painter->restore();

		painter->setFont(option.font);
		painter->translate(crect.left() + gIconSize + gSpacing * 3 + 2, y);
		q->GetTextDocument()->drawContents(painter, rect);
		painter->restore();
		y += textsize.height() + gSpacing;
		++count;
	}
	return ypos + y - crect.top();
}
int MessageDelegate::PaintReaction(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	if (m->GetReactions().empty()) return ypos;
	painter->save();
	QSize size = GetReactionSize(option, index);
	crect.translate(0, ypos);

	int x = crect.left() + gIconSize + gSpacing;
	int y = crect.top();//アイコン、テキスト描画位置は通常よりgSpacing分だけ下げる。
	for (auto& r : m->GetReactions())
	{
		QString name = ":" + r.GetName() + ":";
		auto it = emojicpp::EMOJIS.find(name.toLocal8Bit().data());
		if (it != emojicpp::EMOJIS.end())
			name = (it->second.c_str());
		QString rep = name + " " + QString::number(r.GetUsers().size());
		QFont f;
		f.setPointSizeF(GetReactionPointSize());
		QSize rsize(QFontMetrics(f).boundingRect(rep).size());
		QRect rcrect(x, y, rsize.width() + 2, rsize.height());
		QPainterPath path;
		path.addRoundedRect(rcrect, 10, 10);
		painter->setPen(QPen(Qt::darkGray, 2));
		painter->fillPath(path, QColor(224, 224, 224));
		painter->setFont(f);
		painter->setPen(QPen(Qt::black));
		painter->drawText(rcrect, Qt::TextSingleLine, rep);
		x += rcrect.width() + gSpacing;
	}
	painter->restore();
	return ypos + size.height() + gSpacing;
}
int MessageDelegate::PaintThread(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	if (!m->IsParentMessage()) return ypos;
	painter->save();
	QSize size = GetThreadSize(option, index);
	crect.translate(0, ypos);

	const std::vector<QString>& user = m->GetThread()->GetReplyUsers();
	int x = crect.left() + gIconSize + gSpacing + gSpacing;
	int y = crect.top() + gSpacing;//Threadのアイコン、テキスト描画位置は通常よりgSpacing分だけ下げる。
	for (auto& u : user)
	{
		auto it = gUsers.find(u);
		if (it == gUsers.end())
			painter->drawPixmap(x, y, gEmptyUser->GetPixmap().scaled(QSize(gThreadIconSize, gThreadIconSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		else
			painter->drawPixmap(x, y, it.value().GetPixmap().scaled(QSize(gThreadIconSize, gThreadIconSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		x += gThreadIconSize + gSpacing;
	}
	QFont f(option.font);
	f.setPointSizeF(GetThreadPointSize());
	f.setBold(true);
	painter->setFont(f);
	painter->setPen(QColor(25, 91, 178));
	QString rep = QString::number(m->GetThread()->GetReplies().size()) + " replies";
	QSize rsize(QFontMetrics(f).boundingRect(rep).size());
	QRect rrect(QPoint(x, y + (gThreadIconSize - rsize.height()) / 2), rsize);
	painter->drawText(rrect, Qt::TextSingleLine, rep);
	painter->restore();
	return y + size.height() + gSpacing;
}
int MessageDelegate::PaintDocument(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	const auto& files = m->GetFiles();
	if (files.empty()) return ypos;
	crect.translate(0, ypos);
	int y = crect.top();
	painter->save();

	for (auto& f : files)
	{
		if (f->IsImage())
		{
			ImageFile* i = static_cast<ImageFile*>(f.get());
			auto draw = [](QPainter* painter, QRect crect, int y, const QImage& image)
			{
				QPixmap tmp = QPixmap::fromImage(image);
				painter->setPen(QPen(QBrush(QColor(160, 160, 160)), 2));
				int width = std::min(crect.width() - gSpacing - gIconSize, tmp.width());
				if (width < tmp.width()) tmp = tmp.copy(0, 0, width, tmp.height());
				painter->drawPixmap(crect.left() + gIconSize + gSpacing + 2, y + 2, width, tmp.height(), tmp);
				painter->drawRect(crect.left() + gIconSize + gSpacing + 1, y + 1, width + 2, tmp.height() + 2);
			};

			auto* model = static_cast<MessageListModel*>(mListView->model());
			auto notify_fin_download = [model, index](const AttachedFile*)
			{
				Q_EMIT model->ImageDownloadFinished(index, index, QVector<int>());
			};
			auto draw_immediately = [draw, painter, crect, y](const AttachedFile* file)
			{
				const ImageFile* image = static_cast<const ImageFile*>(file);
				draw(painter, crect, y, image->GetImage().scaledToHeight(gMaxThumbnailHeight, Qt::SmoothTransformation));
			};
			auto draw_loading = [draw, painter, crect, y](const AttachedFile* file)
			{
				draw(painter, crect, y, gTempImage->scaledToHeight(gMaxThumbnailHeight, Qt::SmoothTransformation));
			};
			auto draw_failed = [draw, painter, crect, y](const AttachedFile*)
			{
				//draw(painter, crect, y, gTempImage->scaledToHeight(gMaxThumbnailHeight, Qt::SmoothTransformation));
			};

			i->DownloadAndOpen(draw_immediately, draw_loading, notify_fin_download, draw_failed);

			y += gMaxThumbnailHeight + gSpacing + 4;
		}
		else
		{
			if (f->IsText() || f->IsPDF())
			{
				//一応キャッシュフォルダにダウンロードしておく。
				f->Download(nullptr, nullptr, nullptr, nullptr);
			}
			//画像、テキスト以外は適当に。
			painter->setPen(QPen(QBrush(QColor(160, 160, 160)), 2));
			painter->drawPixmap(crect.left() + gIconSize + gSpacing + gSpacing + 2, y + gSpacing + 2,
								QPixmap::fromImage(gDocIcon->scaledToHeight(gIconSize, Qt::SmoothTransformation)));
			painter->drawRect(crect.left() + gIconSize + gSpacing + 1, y + 1, gMaxThumbnailWidth + 2, gIconSize + gSpacing * 2 + 2);
			QRect rect(crect.left() + gIconSize + gSpacing * 2 + gIconSize + gSpacing, y + gSpacing + 1, gMaxThumbnailWidth, gIconSize + gSpacing * 2);
			QString fsize;
			int s = f->GetFileSize();
			int kmg = 0;
			while (s >= 1024)
			{
				kmg += 1;
				s /= 1024;
				if (kmg >= 3) break;
			}
			switch (kmg)
			{
			case 0: fsize = QString::number(s) + " B"; break;
			case 1: fsize = QString::number(s) + " KB"; break;
			case 2: fsize = QString::number(s) + " MB"; break;
			case 3: fsize = QString::number(s) + " GB"; break;
			}
			QFont font;
			font.setPointSizeF(GetDateTimePointSize());
			QSize size = QFontMetrics(font).boundingRect(f->GetFileName()).size();
			painter->setFont(font);
			painter->setPen(QColor(0, 0, 0));
			painter->drawText(rect, Qt::AlignLeft | Qt::AlignJustify, f->GetFileName());
			rect.translate(0, gIconSize / 2 + 3);
			painter->drawText(rect, Qt::AlignLeft | Qt::AlignJustify, fsize);
			y += gIconSize + gSpacing * 3 + 4;
		}
	}
	painter->restore();
	return y + ypos - crect.top();
}

QString QDateToStr(const QDate& date)
{
	QLocale locale(QLocale("en_US"));
	QString th;
	switch (date.day())
	{
	case 1:
	case 21:
	case 31: th = "st"; break;
	case 2:
	case 22:  th = "nd"; break;
	case 3:
	case 23: th = "rd"; break;
	default: th = "th"; break;
	}
	return locale.toString(date, "dddd, MMMM d'" + th + "' yyyy");
}

int MessageDelegate::PaintSeparator(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QSize size = GetSeparatorSize(option, index);
	QRect rect = crect;
	option.rect.width();
	painter->setPen(QColor(192, 192, 192));

	//左の境界線
	int linewidth = (crect.width() - size.width()) / 2 - gLeftMargin;
	int ycenter = (crect.top() + crect.bottom()) / 2;
	painter->drawLine(0, ycenter, linewidth, ycenter);
	rect.translate(linewidth, 0);

	//枠描画
	painter->setRenderHint(QPainter::Antialiasing);
	painter->drawRoundedRect(rect.left(), rect.top() + 6, size.width() + gLeftMargin + gRightMargin, crect.height() - 12, 11, 11);
	rect.translate(gLeftMargin, gTopMargin);
	painter->setRenderHint(QPainter::Antialiasing, false);

	//日付描画
	QFont f;
	f.setBold(true);
	f.setPointSizeF(GetBasePointSize());
	painter->save();
	painter->setFont(f);
	painter->setPen(QColor(64, 64, 64));
	Message* m = static_cast<Message*>(index.internalPointer());
	painter->drawText(rect, QDateToStr(m->GetTimeStamp().date()));
	rect.translate(size.width() + gRightMargin, 0);
	painter->restore();

	//右の境界線
	painter->drawLine(rect.left(), ycenter, rect.right(), ycenter);
	painter->restore();
	return crect.height();
}

QSize MessageDelegate::GetMessageSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	int width = option.rect.width();

	QSize nmsize = GetNameSize(option, index);
	int height = nmsize.height();

	QSize mssize = GetTextSize(option, index);
	height += mssize.height() + gSpacing;

	return QSize(width, height);
}

QSize MessageDelegate::GetNameSize(const QStyleOptionViewItem&, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	const QString& name = m->GetUser().GetName();
	QFont f;
	f.setBold(true);
	f.setPointSizeF(GetNamePointSize());
	return QFontMetrics(f).boundingRect(name.isEmpty() ? "noname" : name).size();
}
QSize MessageDelegate::GetDateTimeSize(const QStyleOptionViewItem&, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	const QString& dt = m->GetTimeStampStr();
	QFont f;
	f.setPointSizeF(GetDateTimePointSize());
	return QFontMetrics(f).boundingRect(dt).size();
}
QSize MessageDelegate::GetTextSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	QTextDocument* td = m->GetTextDocument();
	int width = option.rect.width() - gRightMargin - gLeftMargin - gSpacing - gIconSize;
	td->setTextWidth(width);
	return QSize(width, ceil(td->documentLayout()->documentSize().height()));
}
QSize MessageDelegate::GetQuoteSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	auto& quotes = m->GetQuotes();
	if (quotes.empty()) return QSize(0, 0);
	int height = 0;
	int width = gMaxThumbnailWidth;
	for (auto& q : quotes)
	{
		if (q->HasImage())
		{
			height += gSpacing + gMaxThumbnailHeight + 4;
		}
		int w = option.rect.width() - gRightMargin - gLeftMargin - gSpacing - gIconSize;
		w = std::min(w, gMaxThumbnailWidth * 2);//テキスト全体の幅
		QTextDocument* td = q->GetTextDocument();
		td->setTextWidth(w - gSpacing * 2 - 2);//テキスト左側の縦線分のスペースを差し引く。
		height += ceil(td->documentLayout()->documentSize().height()) + gSpacing;
		width = std::max(width, w);
	}
	return QSize(width, height - gSpacing);
}
QSize MessageDelegate::GetQuoteTextSize(const QStyleOptionViewItem& option, const QModelIndex& index, int n) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	auto& quotes = m->GetQuotes();
	if (quotes.empty()) return QSize(0, 0);
	auto& q = quotes[n];
	int width = option.rect.width() - gRightMargin - gLeftMargin - gSpacing - gIconSize;
	width = std::min(width, gMaxThumbnailWidth * 2);//テキスト全体の幅
	QTextDocument* td = q->GetTextDocument();
	td->setTextWidth(width - gSpacing * 2 - 2);//テキスト左側の縦線分のスペースを差し引く。
	int height = ceil(td->documentLayout()->documentSize().height());
	return QSize(width, height);
}
QSize MessageDelegate::GetReactionSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	if (m->GetReactions().empty()) return QSize(0, 0);
	int width = option.rect.width() - gRightMargin - gLeftMargin - gSpacing - gIconSize;
	QFont f;
	f.setPointSizeF(GetReactionPointSize());
	int height = QFontMetrics(f).boundingRect("icon").height();
	return QSize(width, height);
}
QSize MessageDelegate::GetThreadSize(const QStyleOptionViewItem&, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	if (!m->IsParentMessage()) return QSize(0, 0);
	int u = (int)m->GetThread()->GetReplyUsers().size();
	return QSize(gThreadIconSize * u + (gSpacing * (u + 1)), gThreadIconSize + gSpacing * 2);
}
QSize MessageDelegate::GetDocumentSize(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	const auto& files = m->GetFiles();
	if (files.empty()) return QSize(0, 0);
	int height = 0;
	int width = gMaxThumbnailWidth;
	for (auto& f : files)
	{
		if (f->IsImage()) height += gMaxThumbnailHeight + 4 + gSpacing;
		//テキストなどその他ドキュメントはサムネイルを生成しないことにする。手間だから。
		else height += gIconSize + gSpacing * 3 + 4;
	}
	return QSize(width, height - gSpacing);
}
QSize MessageDelegate::GetSeparatorSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QFont f;
	f.setBold(true);
	f.setPointSizeF(GetBasePointSize());
	Message* m = static_cast<Message*>(index.internalPointer());
	QSize size = QFontMetrics(f).boundingRect(QDateToStr(m->GetTimeStamp().date())).size();
	return QSize(size.width(), size.height() + gTopMargin + gBottomMargin);
}
