#include <QWidget>
#include <QTextDocument>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include "Message.h"
#include "FileDownloader.h"
#include "emoji.h"

void MrkdwnToHtml_impl(QString& str)
{
	//絵文字
	{
		static const QRegularExpression matchemoji(":[a-zA-Z0-9’._\\+\\-&ãçéíôÅ]+?:");
		auto match = matchemoji.match(str);
		while (match.hasMatch())
		{
			int start = match.capturedStart();
			int end = match.capturedEnd();
			QString emoji = str.mid(start, end - start);
			auto res = emojicpp::EMOJIS.find(emoji.toStdString());
			if (res == emojicpp::EMOJIS.end())
			{
				match = matchemoji.match(str, end);
				continue;
			}
			str.replace(match.captured(), res->second.c_str());
			match = matchemoji.match(str, start + 1);
		}
	}
	//username
	{
		static const QRegularExpression matchuser("<@[a-zA-Z0-9]+>");
		auto match = matchuser.match(str);
		while (match.hasMatch())
		{
			int start = match.capturedStart();
			int end = match.capturedEnd();
			QString user = str.mid(start + 2, end - 3 - start);
			QString user_bk = match.captured(0);
			auto it = gUsers.find(user);
			if (it == gUsers.end())
			{
				match = matchuser.match(str, end);
			}
			else
			{
				const QString& name = it.value().GetName();
				QString to = "<span style=\"color: #195BB2; background-color: #E8F5FA;\">@" + name + "</span>";
				str.replace(user_bk, to);
				match = matchuser.match(str, start + to.size());
			}
		}
	}
	//channel
	{
		static const QRegularExpression matchch("<#[a-zA-Z0-9]+\\|[^>]+>");
		auto match = matchch.match(str);
		while (match.hasMatch())
		{
			int start = match.capturedStart();
			int end = match.capturedEnd();
			QString chan_bk = str.mid(start + 2, end - 3 - start).split("|")[0];
			auto it = std::find_if(gChannelVector.begin(), gChannelVector.end(), [&chan_bk](const Channel& ch) { return ch.GetID() == chan_bk; });
			if (it == gChannelVector.end())
			{
				match = matchch.match(str, end);
			}
			else
			{
				const QString& name = it->GetName();
				QString to = "<font color=\"#195BB2\">#" + name + "</font>";
				str.replace(match.captured(0), to);
				match = matchch.match(str, start + to.size());
			}
		}
	}
	//url
	{
		static const QRegularExpression matchurl("<http(s?):([^\\s]+)>");
		auto match = matchurl.match(str);
		while (match.hasMatch())
		{
			int start = match.capturedStart();
			int end = match.capturedEnd();
			QString url = str.mid(start + 1, end - 2 - start);
			QString escaped_url = url.toHtmlEscaped();
			str.replace(match.captured(), "<a href= \"" + escaped_url + "\">" + url + "</a>");
			match = matchurl.match(str);
		}
	}

	//bold
	static const QRegularExpression bold("(^|[\\s]+)\\*([^\\n]*?)\\*($|[\\s]+)");
	str.replace(bold, "\\1<b>\\2</b>\\3");

	//italic
	static const QRegularExpression italic("(^|[\\s]+)_([^\\n]*?)_($|[\\s]+)");
	str.replace(italic, "\\1<i>\\2</i>\\3");

	//strike
	static const QRegularExpression strike("(^|[\\s]+)~([^\\n]*?)~($|[\\s]+)");
	str.replace(strike, "\\1<s>\\2</s>\\3");

	//箇条書き
	//どうも上手くいかないのでやめる。
	//str.replace(QRegularExpression("(^|\\n)\\*[ ]+([^\\n]*)"), "\\1<ul><li>\\2</li></ul>");
	//str.replace(QRegularExpression("(^|\\n)[0-9]\\. ([^\\n]*)"), "\\1<ol><li>\\2</li></ol>");
	//str.replace(QRegularExpression("</ul>\\s*?<ul>"), "");
	//str.replace(QRegularExpression("</ol>\\s*?<ol>"), "");

	str.replace("\n", "<br>");
}

QString MrkdwnToHtml(const QString& str)
{
	auto s = str.split("```");
	//splitした結果は、必ず
	//0番目...通常
	//1番目...コードブロック
	//2番目...通常
	//︙
	//2n番目...通常
	//2n+1番目...コードブロック
	//となる。
	//コードブロックは単純に表示すべきなので、2n番目だけ置換処理する。

	for (int i = 0; i < s.size(); i += 2)
	{
		MrkdwnToHtml_impl(s[i]);
	}
	for (int i = 1; i < s.size(); i += 2)
	{
		s[i] = "<style> pre { width: 100%; border: 1px solid #000; white-space: pre-wrap; } </style>\n<pre><code><div style=\"background-color:#EDF7FF;\">" + s[i] + "</div></code></pre>";
	}

	return "<font size=\"" + QString::number((int)(GetBasePointSize() * 0.5)) +
		"\"pt>" + s.join("") + "</font>";
}

AttachedFile::AttachedFile(Type type, const QJsonObject& o)
{
	mType = type;
	mFileSize = o.find("size").value().toInt();
	mPrettyType = o.find("pretty_type").value().toString();
	mUrl = o.find("url_private_download").value().toString();
	mID = o.find("id").value().toString();
	mFileName = o.find("name").value().toString();
	mExtension = "." + o.find("filetype").value().toString();
	mUserID = o.find("user").value().toString();
	QDateTime dt;
	dt.setTime_t(o.find("timestamp").value().toInt());
	mTimeStampStr = dt.toString("yyyy/MM/dd hh:mm:ss");
}
ImageFile::ImageFile(const QJsonObject& o)
	: AttachedFile(IMAGE, o)
{}
void ImageFile::RequestDownload(FileDownloader* fd)
{
	if (HasImage())
	{
		Q_EMIT fd->Finished();
		fd->deleteLater();
		return;
	}
	QFile i(mID);
	if (i.exists())
	{
		SetImage(i.readAll());
		Q_EMIT fd->Finished();
		fd->deleteLater();
		return;
	}
	//スロットが呼ばれるタイミングは遅延しているので、
	//gUsersに格納されたあとのUserオブジェクトを渡しておく必要があるはず。
	fd->SetUrl(mUrl);
	QObject::connect(fd, &FileDownloader::Downloaded, [this, fd]()
					 {
						 QByteArray f = fd->GetDownloadedData();
						 QFile o(CachePath("Image", GetID()));
						 o.open(QIODevice::WriteOnly);
						 o.write(f);
						 SetImage(f);
						 Q_EMIT fd->Finished();
						 fd->deleteLater();
					 });
	QObject::connect(fd, &FileDownloader::DownloadFailed, [this, fd]
					 {
						 fd->deleteLater();
					 });
}
bool ImageFile::LoadImage()
{
	if (HasImage()) return true;
	QFile f(CachePath("Image", GetID()));
	if (!f.exists()) return false;
	if (!f.open(QIODevice::ReadOnly)) return false;
	SetImage(f.readAll());
	return true;
}

TextFile::TextFile(const QJsonObject& o)
	: AttachedFile(TEXT, o)
{}
void TextFile::RequestDownload(FileDownloader* fd)
{
	if (HasText())
	{
		Q_EMIT fd->Finished();
		fd->deleteLater();
		return;
	}
	//スロットが呼ばれるタイミングは遅延しているので、
	//gUsersに格納されたあとのUserオブジェクトを渡しておく必要があるはず。
	fd->SetUrl(mUrl);
	QObject::connect(fd, &FileDownloader::Downloaded, [this, fd]()
					 {
						 QByteArray f = fd->GetDownloadedData();
						 QFile o(CachePath("Image", GetID()));
						 o.open(QIODevice::WriteOnly);
						 o.write(f);
						 SetText(f);
						 Q_EMIT fd->Finished();
						 fd->deleteLater();
					 });
	QObject::connect(fd, &FileDownloader::DownloadFailed, [this, fd]
					 {
						 fd->deleteLater();
					 });
}

PDFFile::PDFFile(const QJsonObject& o)
	: AttachedFile(PDF, o)
{}

OtherFile::OtherFile(const QJsonObject& o)
	: AttachedFile(OTHER, o)
{}

Message::Message(Channel::Type type, int ch, const QJsonObject& o)
	: mChannelType(type), mChannel(ch), mRow(0), mThread(nullptr)
{
	mMessage = o.find("text").value().toString();
	mTimeStamp.setTime_t(o.find("ts").value().toString().toDouble());
	mTimeStampStr = mTimeStamp.toString("yyyy/MM/dd hh:mm:ss");

	auto sub = o.find("subtype");

	if (sub.value() == "bot_message")
	{
		auto it = o.find("bot_id");
		mUserID = it.value().toString();
	}
	else if (sub.value() == "file_comment")
	{
		auto it = o.find("comment");
		mUserID = it.value().toObject().find("user").value().toString();
	}
	else
		//if (sub == o.end() ||
		//sub.value() == "channel_join" ||
		//sub.value() == "channel_purpose" ||
		//sub.value() == "pinned_item" ||
		//sub.value() == "channel_name")
	{
		auto it = o.find("user");
		if (it != o.end()) mUserID = it.value().toString();
		//どうもエクスポートしたデータが破損しているケースがあるらしい。
		//「メッセージがあった」という記録のみがあって、メッセージの内容やユーザーなどの必須情報が一切欠落している。
		//意味がわからんがどうしようもないので、ユーザー情報は空にしておく。
	}

	auto file = o.find("files");
	if (file != o.end())
	{
		//mFiles = std::make_unique<AttachedFiles>(file.value().toArray());
		QJsonArray ar = file.value().toArray();
		mFiles.reserve(ar.size());
		for (const auto& a : ar)
		{
			const QJsonObject& f = a.toObject();
			const QString& type = f.find("mimetype").value().toString();
			if (type.contains("text")) mFiles.emplace_back(std::make_unique<TextFile>(f));
			else if (type.contains("image")) mFiles.emplace_back(std::make_unique<ImageFile>(f));
			else if (type.contains("pdf")) mFiles.emplace_back(std::make_unique<PDFFile>(f));
			else  mFiles.emplace_back(std::make_unique<OtherFile>(f));
		}
	}

	auto reactions = o.find("reactions");
	if (reactions != o.end())
	{
		auto ar = reactions.value().toArray();
		mReactions.reserve(ar.size());
		for (auto a : ar)
		{
			QJsonObject ro = a.toObject();
			QJsonArray uar = ro["users"].toArray();
			QStringList users;
			users.reserve(uar.size());
			for (auto u : uar)
				users.append(u.toString());
			mReactions.emplace_back(ro.find("name").value().toString(), std::move(users));
		}
	}
}
Message::Message(Channel::Type type, int ch, const QJsonObject& o, QString threads_ts)
	: Message(type, ch, o)
{
	mThreadTimeStampStr = std::move(threads_ts);
}
void Message::CreateTextDocument() const
{
	mTextDocument = std::make_unique<QTextDocument>();
	mTextDocument->setHtml(GetHtmlMessage());
}
bool Message::IsReply() const
{
	if (mThread == nullptr) return false;
	if (mThread->GetParent() == this) return false;
	return true;
}
bool Message::IsParentMessage() const
{
	if (mThread == nullptr) return false;
	if (mThread->GetParent() != this) return false;
	return true;
}

Thread::Thread(const Message* parent, std::vector<QString>&& users)
	: mParent(parent), mReplyUsers(std::move(users))
{}
