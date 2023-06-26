#ifndef MESSAGE_H
#define MESSAGE_H

#include <optional>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QImage>
#include <QTextDocumentFragment>
#include "GlobalVariables.h"
#include "AttachedFile.h"

QString MrkdwnToHtml(const QString& str);

class Thread;
class Quote;

struct ValueTo
{
	ValueTo(const QString& filename, qsizetype index)
		: filename_(filename), index_(index)
	{}

	QString string(const QJsonObject::const_iterator& it) const
	{
		const auto& v = it.value();
		if (!v.isString())
			throw FatalError(QString("key \"") + it.key() + "\" in the " + QString::number(index_) + "th message of \"" + filename_ +
				"\" is not a string.");
		return v.toString();
	}
	QJsonArray array(const QJsonObject::const_iterator& it) const
	{
		const auto& v = it.value();
		if (!v.isArray())
			throw FatalError(QString("key \"") + it.key() + "\" in the " + QString::number(index_) + "th message of \"" + filename_ +
				"\" is not an array.");
		return v.toArray();
	}
	QJsonObject object(const QJsonObject::const_iterator& it) const
	{
		const auto& v = it.value();
		if (!v.isObject())
			throw FatalError(QString("key \"") + it.key() + "\" in the " + QString::number(index_) + "th message of \"" + filename_ +
				"\" is not an object.");
		return v.toObject();
	}

private:
	const QString& filename_;
	qsizetype index_;
};
struct FindKeyAs
{
	FindKeyAs(const QString& filename, qsizetype index)
		: value_to(filename, index), filename_(filename), index_(index)
	{}

	//この関数群、QJsonObjectやQJsonValue、QJsonValueRefあたりを返り値にして、呼び出し元で返り値に対してtoStringを呼ぶと、
	//どういうわけか例外を投げてクラッシュする。
	//挙動がQt5と6によって異なっており、またReleaseだと発生しなかったりもするらしい。
	//原因がわからないが、どうもQJsonValueRefのコピーコンストラクタあたりが何か悪さをしている気がするので、
	//コピーを避けるためにとりあえずstringとarrayの2つの関数を用意しておく。

	template <class ...Strs>
	QString string(const QJsonObject& o, const Strs& ...keys) const
	{
		return get(o, [this](const auto& v) { return value_to.string(v); }, keys...);
	}
	template <class ...Strs>
	QJsonArray array(const QJsonObject& o, const Strs& ...keys) const
	{
		return get(o, [this](const auto& v) { return value_to.array(v); }, keys...);
	}

private:
	template <class Ret, class Str, class ...Strs>
	auto get(const QJsonObject& o, Ret to, const Str& key, const Strs& ...keys) const
	{
		QJsonObject::const_iterator it = o.find(key);
		if (it == o.end())
			throw FatalError(QString("key \"") + key + "\" not found in the " + QString::number(index_) + "th message of \"" + filename_ + "\".");
		if constexpr (sizeof...(Strs) != 0)
			return get(value_to.object(it), to, keys...);
		else
			return to(it);
	}
	ValueTo value_to;
	const QString& filename_;
	qsizetype index_;
};

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

	Message(const QString& filename, qsizetype index, Channel::Type, int ch, const QJsonObject& o);
	Message(const QString& filename, qsizetype index, Channel::Type, int ch, const QJsonObject& o, QString threads_ts);//リプライ用。

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
	std::vector<std::unique_ptr<Quote>>& GetQuotes() { return mQuotes; }
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
	std::vector<std::unique_ptr<Quote>> mQuotes;
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

class Quote
{
public:

	Quote(const QString& message_ts, int index,
		  const QString& service_name, const QString& siurl,
		  const QString& title, const QString& link,
		  const QString& text, const QString& imurl);

	const QString& GetServiceName() const { return mServiceName; }
	const QString& GetServiceIconUrl() const { return mServiceIconUrl; }
	const QString& GetTitle() const { return mTitle; }
	const QString& GetLink() const { return mLink; }
	const QString& GetText() const { return mText; }
	const QTextDocument* GetTextDocument() const
	{
		if (!mTextDocument) CreateTextDocument();
		return mTextDocument.get();
	}
	QTextDocument* GetTextDocument()
	{
		if (!mTextDocument) CreateTextDocument();
		return mTextDocument.get();
	}
	const QString& GetImageUrl() const { return mImageUrl; }
	const ImageFile* GetImageFile() const { return mImage.get(); }
	ImageFile* GetImageFile() { return mImage.get(); }

	bool HasServiceName() const { return !mServiceName.isEmpty(); }
	bool HasTitle() const { return !mTitle.isEmpty(); }
	bool HasLink() const { return !mLink.isEmpty(); }
	bool HasText() const { return !mText.isEmpty(); }
	bool HasImage() const { return !mImageUrl.isEmpty(); }

	template <class FAlready, class FWait, class FSucceeded, class FFailed>
	void DownloadAndOpenImage(FAlready already, FWait wait, FSucceeded succeeded, FFailed failed)
	{
		mImage->DownloadAndOpen(already, wait, succeeded, failed);
	}

private:

	void CreateTextDocument() const;

	//const Message* mParent;
	QString mServiceName;
	QString mServiceIconUrl;
	QString mTitle;
	QString mLink;
	QString mText;
	mutable std::unique_ptr<QTextDocument> mTextDocument;
	QString mImageUrl;
	std::unique_ptr<ImageFile> mImage;
};

#endif