#ifndef MESSAGELISTWIDGET_H
#define MESSAGELISTWIDGET_H

#include <QListView>
#include <QAbstractItemModel>
#include <QTextDocument>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QThreadPool>
#include "GlobalVariables.h"

QString MrkdwnToHtml(const QString& str);

class ImageFile;
class TextFile;
class OtherFile;

class AttachedFile
{
public:

	enum Type { TEXT, IMAGE, OTHER, };

	AttachedFile(Type type, const QJsonObject& o);

	Type GetType() const { return mType; }
	bool IsText() const { return mType == TEXT; }
	bool IsImage() const { return mType == IMAGE; }
	bool IsOther() const { return mType == OTHER; }

	int GetFileSize() const { return mFileSize; }
	const QString& GetID() const { return mID; }
	const QString& GetPrettyType() const { return mPrettyType; }
	const QString& GetUrl() const { return mUrl; }
	const QString& GetFileName() const { return mFileName; }
	const QString& GetUserID() const { return mUserID; }
	const QString& GetTimeStampStr() const { return mTimeStampStr; }

private:

	Type mType;
	int mFileSize;
	QString mID;
	QString mPrettyType;
	QString mUrl;
	QString mFileName;
	QString mUserID;
	QString mTimeStampStr;
};
class ImageFile : public AttachedFile
{
public:
	ImageFile(const QJsonObject& o);
	void SetImage(const QByteArray& data)
	{
		mImage.loadFromData(data);
	}
	const QImage& GetImage() const { return mImage; }
	void ClearImage() { mImage = QImage(); }
	bool HasImage() const { return !mImage.isNull(); }
private:
	QImage mImage;
};
class TextFile : public AttachedFile
{
public:
	TextFile(const QJsonObject& o);
};
class OtherFile : public AttachedFile
{
public:
	OtherFile(const QJsonObject& o);
};

class AttachedFiles
{
public:
	AttachedFiles(const QJsonArray& o);

	std::vector<std::unique_ptr<AttachedFile>>& GetFiles() { return mFiles; }
	const std::vector<std::unique_ptr<AttachedFile>>& GetFiles() const { return mFiles; }

private:
	std::vector<std::unique_ptr<AttachedFile>> mFiles;

};


class Thread;

class Reaction
{
public:

	Reaction(const QString& name, const QStringList& users)
		: mName(name), mUsers(users)
	{}
	Reaction(QString&& name, QStringList&& users)
		: mName(std::move(name)), mUsers(std::move(users))
	{}

	const QString& GetName() const { return mName; }
	const QStringList& GetUsers() const { return mUsers; }

private:
	QString mName;
	QStringList mUsers;
};

class Message
{
public:

	Message(int ch, int row, const QJsonObject& o);
	Message(int ch, int row, const QJsonObject& o, Thread& thread);

	int GetChannel() const { return mChannel; }
	int GetRow() const { return mRow; }
	const QString& GetUserID() const { return mUserID; }
	const QString& GetMessage() const { return mMessage; }
	const QString& GetTimeStampStr() const { return mTimeStampStr; }
	const QDateTime& GetTimeStamp() const { return mTimeStamp; }
	const User& GetUser() const
	{
		auto it = gUsers.find(mUserID);
		if (it == gUsers.end()) return *gEmptyUser;
		return it.value();
	}
	bool HasTextDocument() const { return mTextDocument != nullptr; }
	QTextDocument* GetTextDocument()
	{
		if (!mTextDocument) CreateTextDocument();
		return mTextDocument.get();
	}
	const QTextDocument* GetTextDocument() const
	{
		if (!mTextDocument) CreateTextDocument();
		return mTextDocument.get();
	}
	void ClearTextDocument()
	{
		if (mTextDocument) mTextDocument.reset();
	}

	Thread* GetThread() { return mThread; }
	const Thread* GetThread() const { return mThread; }
	const AttachedFiles* GetFiles() const { return mFiles.get(); }
	AttachedFiles* GetFiles() { return mFiles.get(); }
	const std::vector<Reaction>& GetReactions() const { return mReactions; }

	bool IsReply() const;
	bool IsParentMessage() const;

private:

	void CreateTextDocument() const;

	int mChannel;
	int mRow;
	QString mUserID;
	QString mMessage;
	QString mTimeStampStr;
	QDateTime mTimeStamp;
	mutable std::unique_ptr<QTextDocument> mTextDocument;

	std::vector<Reaction> mReactions;
	//自分自身が通常のメッセージである場合、mThreadはスレッドを持つのならそのポインタ、持たないのならnullptr、
	//自分自身がスレッド内の返信である場合、自分自身を格納するThreadへのポインタが格納されている。
	//自分が通常のメッセージか返信家はIsReply関数で識別できる。
	Thread* mThread;
	std::unique_ptr<AttachedFiles> mFiles;
};

class Thread
{
public:

	Thread(std::vector<QString>&& users);
	void SetParent(const Message* m) { mParent = m; }
	const Message* GetParent() const { return mParent; }
	void AddReply(int ch, const QJsonObject& o) { mReplies.emplace_back(std::make_shared<Message>(ch, (int)mReplies.size(), o, *this)); }
	const std::vector<std::shared_ptr<Message>>& GetReplies() const { return mReplies; }
	const std::vector<QString>& GetReplyUsers() const { return mReplyUsers; }

private:

	const Message* mParent;
	std::vector<std::shared_ptr<Message>> mReplies;
	std::vector<QString> mReplyUsers;
};

class QLabel;

class ImageWidget : public QLabel
{
	Q_OBJECT
public:
	ImageWidget(const ImageFile* i, int pwidth);

signals:

	void clicked(const ImageFile*);

private:

	virtual void mousePressEvent(QMouseEvent* event) override;

	const ImageFile* mImage;

};

class DocumentWidget : public QFrame
{
	Q_OBJECT
public:

	DocumentWidget(const AttachedFile* file, int pwidth);

signals:

	void clicked(const AttachedFile*);

private:

	virtual void mousePressEvent(QMouseEvent* event) override;

	const AttachedFile* mFile;

};

class ThreadWidget : public QFrame
{
	Q_OBJECT
public:

	ThreadWidget(Message& m, QSize threadsize);

signals:

	void clicked(const Message*);

private:

	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void enterEvent(QEvent* evt) override;
	void leaveEvent(QEvent* evt) override;

	QLabel* mViewMessage;
	const Message* mParentMessage;
};

class MessageListModel;
class MessageDelegate;

QWidget* CreateMessageWidget(Message& m, QSize namesize, QSize datetimesize, QSize textsize, QSize threadsize, int pwidth, bool has_thread);

class MessageListView : public QListView
{
	Q_OBJECT
public:

	MessageListView();

	void Construct(const QString& channel);
	bool IsConstructed() const { return mConstructed; }

	//clear関数は生成されたQTextDocumentと付随するImageFileを削除しメモリを解放する。
	void Clear();

	friend class MessageListModel;
	friend class MessageDelegate;

	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void wheelEvent(QWheelEvent* event) override;
	virtual void leaveEvent(QEvent* event) override;

	void SetMessages(std::vector<std::shared_ptr<Message>>&& m) { mMessages = std::move(m); }
	const std::vector<std::shared_ptr<Message>>& GetMessages() const { return mMessages; }

	int GetNumOfPages() const { return (int)mMessages.size() / gSettings->value("NumOfMessagesPerPage").toInt() + 1; }
	int GetCurrentRow();
	int GetCurrentPage();

public slots:

	bool ScrollToRow(int row);
	bool JumpToPage(int page);
	void UpdateCurrentPage();

signals:

	void CurrentPageChanged(int);

protected:

	QModelIndex mPreviousIndex;
	int mPreviousPage;
	bool mConstructed;
	std::vector<std::shared_ptr<Message>> mMessages;
	std::map<QString, Thread> mThreads;//thread_tsをキーとしている。
};

class FileDownloader;

class MessageListModel : public QAbstractListModel
{
	Q_OBJECT
public:

	MessageListModel(QListView* list);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	//virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	virtual bool canFetchMore(const QModelIndex& parent) const override;
	virtual void fetchMore(const QModelIndex& parent) override;

	virtual int GetTrueRowCount() const;

public slots:

	virtual void Open(const std::vector<std::shared_ptr<Message>>* m);
	virtual void DownloadImage(const QModelIndex& index, ImageFile& image);
	virtual void SetDownloadedImage(FileDownloader* fd, const QModelIndex& index, ImageFile& image);

signals:

	//本当は引数は最初の一つだけで十分だが、dataChangedに繋ぐのでそちらの分だけ用意しておく。
	void ImageDownloadFinished(const QModelIndex& index1, const QModelIndex& index2, const QVector<int>&);

protected:

	QListView* mListView;
	const std::vector<std::shared_ptr<Message>>* mMessages;
	int mSize;
};

class SlackLogViewer;

class MessageDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:

	MessageDelegate(QListView* list);

	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

	int PaintMessage(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight
	int PaintReaction(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight
	int PaintThread(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight
	int PaintDocument(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight

	QSize GetMessageSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QSize GetNameSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	QSize GetDateTimeSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	QSize GetTextSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QSize GetReactionSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QSize GetThreadSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	QSize GetDocumentSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

protected:

	QListView* mListView;
};

#endif