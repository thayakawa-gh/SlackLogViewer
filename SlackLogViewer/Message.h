#ifndef MESSAGE_H
#define MESSAGE_H

#include <optional>
#include <QString>
#include <QJsonObject>
#include <QStringList>
#include <QImage>
#include <QTextDocumentFragment>
#include "GlobalVariables.h"
#include "AttachedFile.h"

QString MrkdwnToHtml(const QString& str);

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

	Message(Channel::Type, int ch, const QJsonObject& o);
	Message(Channel::Type, int ch, const QJsonObject& o, QString threads_ts);//リプライ用。

	Message(Channel::Type, int ch, const QDateTime& datetime);//日付のセパレータ用。

	Channel::Type GetChannelType() const { return mChannelType; }
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

	Thread* GetThread() { return mThread; }
	const Thread* GetThread() const { return mThread; }
	const QString& GetThreadTimeStampStr() const { return mThreadTimeStampStr; }
	void SetThread(Thread* th) { mThread = th; }

	const std::vector<std::unique_ptr<AttachedFile>>& GetFiles() const { return mFiles; }
	std::vector<std::unique_ptr<AttachedFile>>& GetFiles() { return mFiles; }
	const std::vector<Reaction>& GetReactions() const { return mReactions; }

	bool IsReply() const;
	bool IsParentMessage() const;
	bool IsSeparator() const;

private:

	void CreateTextDocument() const;

	Channel::Type mChannelType;
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
	//MessageListView、Message、Threadなどで複雑に共有されうるmThreadをstd::shared_ptrにするべきではない。
	//std::weak_ptrと悩ましいが、Messagesが生きている限りThreadが死なないのは明らかなので、とりあえず生ポインタにしておく。
	Thread* mThread;
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