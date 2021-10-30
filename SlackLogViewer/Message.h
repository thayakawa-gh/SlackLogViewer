#ifndef MESSAGE_H
#define MESSAGE_H

#include <optional>
#include <QString>
#include <QJsonObject>
#include <QStringList>
#include <QImage>
#include <QTextDocumentFragment>
#include "GlobalVariables.h"

class FileDownloader;

QString MrkdwnToHtml(const QString& str);

inline bool Contains(const QString& src, const QString& phrase, bool case_)
{
	return src.contains(phrase, case_ ? Qt::CaseSensitive : Qt::CaseInsensitive);
}
inline bool Contains(const QString& src, const QStringList& keys, bool case_, bool all)
{
	bool found = all;
	for (const auto& k : keys)
	{
		bool contains = src.contains(k, case_ ? Qt::CaseSensitive : Qt::CaseInsensitive);
		//QTextCursor c = d->find(k, 0, f);
		if (all)
		{
			//一つでも含まれなかった場合は見つからなかった扱い。
			if (!contains)
			{
				found = false;
				break;
			}
		}
		else
		{
			//一つでも含まれればOK。
			if (contains)
			{
				found = true;
				break;
			}
		}
	}
	return found;
}
inline bool Contains(const QString& src, const QRegularExpression& regex)
{
	return src.contains(regex);
}

class AttachedFile
{
public:

	enum Type { TEXT, IMAGE, PDF, OTHER, };

	AttachedFile(Type type, const QJsonObject& o);

	Type GetType() const { return mType; }
	bool IsText() const { return mType == TEXT; }
	bool IsImage() const { return mType == IMAGE; }
	bool IsPDF() const { return mType == PDF; }
	bool IsOther() const { return mType == OTHER; }

	int GetFileSize() const { return mFileSize; }
	const QString& GetID() const { return mID; }
	const QString& GetPrettyType() const { return mPrettyType; }
	const QString& GetUrl() const { return mUrl; }
	const QString& GetFileName() const { return mFileName; }
	const QString& GetUserID() const { return mUserID; }
	const QString& GetTimeStampStr() const { return mTimeStampStr; }

	bool FileNameContains(const QString& phrase, bool case_) const
	{
		return ::Contains(GetFileName(), phrase, case_);
	}
	bool FileNameContains(const QStringList& keys, bool case_, bool all) const
	{
		return ::Contains(GetFileName(), keys, case_, all);
	}
	bool FileNameContains(const QRegularExpression& regex) const
	{
		return ::Contains(GetFileName(), regex);
	}
	virtual bool FileContentsContains(const QString& /*phrase*/, bool /*case_*/) const { return false; }
	virtual bool FileContentsContains(const QStringList& /*keys*/, bool /*case_*/, bool /*all*/) const { return false; }
	virtual bool FileContentsContains(const QRegularExpression& /*regex*/) const { return false; }

protected:

	Type mType;
	int mFileSize;
	QString mID;
	QString mPrettyType;
	QString mUrl;
	QString mFileName;
	QString mExtension;
	QString mUserID;
	QString mTimeStampStr;
};
class ImageFile : public AttachedFile
{
public:

	ImageFile(const QJsonObject& o);
	const QImage& GetImage() const { return mImage; }
	void ClearImage() { mImage = QImage(); }
	bool HasImage() const { return !mImage.isNull(); }
	void RequestDownload(FileDownloader* fd);
	bool LoadImage();

private:

	void SetImage(const QByteArray& data)
	{
		mImage.loadFromData(data);
	}

	QImage mImage;
};
class TextFile : public AttachedFile
{
public:

	TextFile(const QJsonObject& o);
	void SetText(const QByteArray& data)
	{
		mText = data;
	}
	void ClearText() { mText.reset(); }
	bool HasText() const { return mText.has_value(); }
	void RequestDownload(FileDownloader* fd);

private:
	std::optional<QByteArray> mText;
};
class PDFFile : public AttachedFile
{
public:

	PDFFile(const QJsonObject& o);

private:
};
class OtherFile : public AttachedFile
{
public:
	OtherFile(const QJsonObject& o);
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

	Message(int ch, const QJsonObject& o);
	Message(int ch, const QJsonObject& o, QString threads_ts);//これはリプライ用。

	int GetChannel() const { return mChannel; }
	int GetRow() const { return mRow; }
	void SetRow(int row) { mRow = row; }
	const QString& GetUserID() const { return mUserID; }
	const QString& GetMessage() const { return mMessage; }
	const QString& GetHtmlMessage() const
	{
		if (mHtmlMessage.isEmpty()) mHtmlMessage = MrkdwnToHtml(mMessage);
		return mHtmlMessage;
	}
	const QString& GetPlainMessage() const
	{
		if (mPlainMessage.isEmpty()) mPlainMessage = QTextDocumentFragment::fromHtml(GetHtmlMessage()).toPlainText();
		return mPlainMessage;
	}
	const QString& GetTimeStampStr() const { return mTimeStampStr; }
	const QDateTime& GetTimeStamp() const { return mTimeStamp; }
	const User& GetUser() const
	{
		auto it = gUsers.find(mUserID);
		if (it == gUsers.end())
			return *gEmptyUser;
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
	bool Contains(const QString& phrase, bool case_, bool body, bool user, bool fname, bool /*fcont*/)
	{
		if (body && ::Contains(GetPlainMessage(), phrase, case_)) return true;
		if (user && ::Contains(gUsers[GetUserID()].GetName(), phrase, case_)) return true;
		if (fname && !mFiles.empty())
		{
			for (auto& f : GetFiles())
			{
				if (f->FileNameContains(phrase, case_)) return true;
			}
		}
		return false;
	}
	bool Contains(const QStringList& keys, bool case_, bool all, bool body, bool user, bool fname, bool /*fcont*/)
	{
		if (body && ::Contains(GetPlainMessage(), keys, case_, all)) return true;
		if (user && ::Contains(gUsers[GetUserID()].GetName(), keys, case_, all)) return true;
		if (fname)
		{
			for (auto& f : GetFiles())
			{
				if (f->FileNameContains(keys, case_, all)) return true;
			}
		}
		return false;
	}
	bool Contains(const QRegularExpression& regex, bool body, bool user, bool fname, bool /*fcont*/)
	{
		if (body && ::Contains(GetPlainMessage(), regex)) return true;
		if (user && ::Contains(gUsers[GetUserID()].GetName(), regex)) return true;
		if (fname)
		{
			for (auto& f : GetFiles())
			{
				if (f->FileNameContains(regex)) return true;
			}
		}
		return false;
	}

	Thread* GetThread() { return mThread.get(); }
	const Thread* GetThread() const { return mThread.get(); }
	const QString& GetThreadTimeStampStr() const { return mThreadTimeStampStr; }
	void SetThread(const std::shared_ptr<Thread>& th) { mThread = th; }

	const std::vector<std::unique_ptr<AttachedFile>>& GetFiles() const { return mFiles; }
	std::vector<std::unique_ptr<AttachedFile>>& GetFiles() { return mFiles; }
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
	mutable QString mHtmlMessage;
	mutable QString mPlainMessage;
	mutable std::unique_ptr<QTextDocument> mTextDocument;

	std::vector<Reaction> mReactions;
	//自分自身が通常のメッセージである場合、mThreadはスレッドを持つのならそのポインタ、持たないのならnullptr、
	//自分自身がスレッド内の返信である場合、自分自身を格納するThreadへのポインタが格納されている。
	//自分が通常のメッセージか返信家はIsReply関数で識別できる。
	QString mThreadTimeStampStr;
	std::shared_ptr<Thread> mThread;
	std::vector<std::unique_ptr<AttachedFile>> mFiles;
};

class Thread
{
public:

	Thread(const Message* parent, std::vector<QString>&& users);
	//void SetParent(const Message* m) { mParent = m; }
	const Message* GetParent() const { return mParent; }
	//void AddReply(int ch, const QJsonObject& o) { mReplies.emplace_back(std::make_shared<Message>(ch, (int)mReplies.size(), o, *this)); }
	void AddReply(const std::shared_ptr<Message>& m) { m->SetRow((int)mReplies.size()); mReplies.emplace_back(m); }
	const std::vector<std::shared_ptr<Message>>& GetReplies() const { return mReplies; }
	const std::vector<QString>& GetReplyUsers() const { return mReplyUsers; }

private:

	const Message* mParent;
	std::vector<std::shared_ptr<Message>> mReplies;
	std::vector<QString> mReplyUsers;
};

#endif