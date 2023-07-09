#include <QWidget>
#include <QTextDocument>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QException>
#include "Message.h"
#include "AttachedFile.h"
#include "emoji.h"
#include <optional>

//絵文字、ユーザー名、チャンネル名、urlを置き換える。
//これらはSlack独自表記になっている。
void ReplaceSlackNotation(QString& str)
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
}

void MrkdwnToHtml_impl(QString& str)
{
	ReplaceSlackNotation(str);

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

	return /*"<font size=\"" + QString::number((int)(GetBasePointSize() * 0.5)) +
		/*"\"pt>" + */s.join("")/* + "</font>"*/;
}

Message::Message(const QString& filename, qsizetype index, Channel::Type type, int ch, const QJsonObject& o)
	: mChannelType(type), mChannel(ch), mRow(0), mThread(nullptr)
{
	mMessage = o.find("text").value().toString();
	QString ts = o.find("ts").value().toString();
	mTimeStamp.setSecsSinceEpoch(ts.toDouble());
	mTimeStampStr = mTimeStamp.toString("yyyy/MM/dd hh:mm:ss");

	auto get_as = FindKeyAs(filename, index);
	auto value_to = ValueTo(filename, index);

	auto sub = o.find("subtype");

	if (sub != o.end() && value_to.string(sub) == "bot_message")
	{
		mUserID = get_as.string(o, "bot_id");
	}
	else if (sub != o.end() && value_to.string(sub) == "file_comment")
	{
		//auto com = find_key(o, "comment").toObject();
		mUserID = get_as.string(o, "comment", "user");
	}
	else
	{
		//どうもエクスポートしたデータが破損しているケースがあるらしい。
		//「メッセージがあった」という記録のみがあって、メッセージの内容やユーザーなどの必須情報が一切欠落している。
		//意味がわからんがどうしようもないので、ユーザー情報は空にしておく。
		auto it = o.find("user");
		if (it != o.end()) mUserID = value_to.string(it);
	}

	auto file = o.find("files");
	if (file != o.end())
	{
		//mFiles = std::make_unique<AttachedFiles>(file.value().toArray());
		QJsonArray ar = value_to.array(file);
		mFiles.reserve(ar.size());
		for (const auto& a : ar)
		{
			QJsonObject f = a.toObject();
			auto it = f.find("mimetype");
			QString ftype = it == f.end() ? "" : it->toString();
			if (ftype.contains("text")) mFiles.emplace_back(std::make_unique<TextFile>(f));
			else if (ftype.contains("image")) mFiles.emplace_back(std::make_unique<ImageFile>(f));
			else if (ftype.contains("pdf")) mFiles.emplace_back(std::make_unique<PDFFile>(f));
			else  mFiles.emplace_back(std::make_unique<OtherFile>(f));
		}
	}

	auto quotes = o.find("attachments");
	if (quotes != o.end())
	{
		QJsonArray ar = value_to.array(quotes);
		mQuotes.reserve(ar.size());
		int index = 0;
		for (const auto& a : ar)
		{
			QJsonObject q = a.toObject();
			QString service_name;
			if (auto it = q.find("service_name"); it != q.end()) service_name = value_to.string(it);
			QString service_icon;
			if (auto it = q.find("service_icon"); it != q.end()) service_icon = value_to.string(it);
			QString title;
			if (auto it = q.find("title"); it != q.end()) title = value_to.string(it);
			QString title_link;
			if (auto it = q.find("from_url"); it != q.end()) title_link = value_to.string(it);
			if (auto it = q.find("title_link"); it != q.end() && title_link.isEmpty()) title_link = value_to.string(it);
			QString text;
			if (auto it  = q.find("text"); it != q.end()) text = value_to.string(it);
			QString image_url;
			if (auto it = q.find("image_url"); it != q.end()) image_url = value_to.string(it);

			mQuotes.emplace_back(
				std::make_unique<Quote>(ts, index, service_name, service_icon, title, title_link, text, image_url));
			++index;
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
			QJsonArray uar = get_as.array(ro, "users");
			QStringList users;
			users.reserve(uar.size());
			for (auto u : uar)
				users.append(u.toString());
			mReactions.emplace_back(get_as.string(ro, "name"), std::move(users));
		}
	}
}
Message::Message(const QString& filename, qsizetype index, Channel::Type type, int ch, const QJsonObject& o, QString threads_ts)
	: Message(filename, index, type, ch, o)
{
	mThreadTimeStampStr = std::move(threads_ts);
}
Message::Message(Channel::Type type, int ch, const QDateTime& datetime)
	: mChannelType(type), mChannel(ch), mTimeStamp(datetime), mRow(0), mThread(nullptr)
{}

void Message::CreateTextDocument() const
{
	mTextDocument = std::make_unique<QTextDocument>();
	QFont f;
	f.setPointSizeF(GetMessagePointSize());
	mTextDocument->setDefaultFont(f);
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
bool Message::IsSeparator() const
{
	//Separatorの場合、Jsonの解析を行っていないので、
	//mTimeStampStr
	//mUserID
	//mHtmlMessage
	// 
	//などが空になっている。
	if (mTimeStampStr.isEmpty()) return true;
	return false;
}

Thread::Thread(const Message* parent, std::vector<QString>&& users)
	: mParent(parent), mReplyUsers(std::move(users))
{}

Quote::Quote(const QString& message_ts, int index,
			 const QString& service_name, const QString& siurl,
			 const QString& title, const QString& link,
			 const QString& text, const QString& imurl)
	: mServiceName(service_name), mServiceIconUrl(siurl),
	mTitle(title), mLink(link),
	mText(text), mImageUrl(imurl)
{
	if (!mImageUrl.isEmpty()) mImage = std::make_unique<ImageFile>(mImageUrl, message_ts, index);
}
void Quote::CreateTextDocument() const
{
	QString html;
	//html += "<font size=\"" + QString::number(GetDateTimePointSize()) + "\"pt>";

	if (HasServiceName()) html += "<b>" + GetServiceName() + "</b><br>";

	if (HasTitle())
	{
		QString escaped_url = GetLink().toHtmlEscaped();
		html += "<a href= \"" + escaped_url + "\"><b>" + GetTitle() + "</b></a><br>";
	}
	if (HasText())
	{
		auto text = GetText();
		ReplaceSlackNotation(text);
		html += text;
	}
	//html += "</font>";
	mTextDocument = std::make_unique<QTextDocument>();
	QFont f;
	f.setPointSizeF(GetMessagePointSize());
	mTextDocument->setDefaultFont(f);
	mTextDocument->setHtml(html);
}