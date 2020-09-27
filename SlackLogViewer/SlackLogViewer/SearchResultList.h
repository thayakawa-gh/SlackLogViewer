#ifndef SEARCHRESULTVIEW_H
#define SEARCHRESULTVIEW_H

#include "MessageListView.h"

struct SearchMode
{
	enum Match : char { EXACTPHRASE, ALLWORDS, ANYWORDS, };
	enum Case : char { INSENSITIVE, SENSITIVE, };
	enum Regex : char { KEYWORDS, REGEX, };
	enum Range : char { ALLCHS, VISIBLECHS, CURRENTCH, };
	SearchMode(Match match, Case case_, Regex regex, Range range)
		: mValue(0)
	{
		mValue |= (char)match;
		mValue |= (char)case_ << 2;
		mValue |= (char)regex << 4;
		mValue |= (char)range << 6;
	}
	SearchMode()
		: SearchMode(EXACTPHRASE, INSENSITIVE, KEYWORDS, CURRENTCH)
	{}

	bool operator==(SearchMode mode) const { return mValue == mode.mValue; }
	bool operator!=(SearchMode mode) const { return !(*this == mode); }

	Match GetMatchMode() const { return (Match)(mValue & 0b11); }
	Case GetCaseMode() const { return (Case)((mValue & 0b1100) >> 2); }
	Regex GetRegexMode() const { return (Regex)((mValue & 0b110000) >> 4); }
	Range GetRangeMode() const { return (Range)((mValue & 0b11000000) >> 6); }

private:

	short mValue;
};

std::vector<std::shared_ptr<Message>> SearchExactPhrase(int ch, MessageListView* mes, const QString& phrase, SearchMode::Case case_);
std::vector<std::shared_ptr<Message>> SearchWords(int ch, MessageListView* mes, const QStringList& keys, SearchMode::Match match, SearchMode::Case case_);
std::vector<std::shared_ptr<Message>> SearchWithRegex(int ch, MessageListView* mes, const QRegularExpression& regex);

class QStackedWidget;

class SearchResultListView : public QWidget
{
	Q_OBJECT
public:

	SearchResultListView();

	size_t Search(int ch, const QStackedWidget* stack, const QString& key, SearchMode mode);

	const std::vector<std::shared_ptr<Message>>& GetMessages() const { return mView->GetMessages(); }

	MessageListView* GetView() { return mView; }

signals:

	void Closed();

protected:

	QLabel* mLabel;
	MessageListView* mView;
	SearchMode mMode;
	QString mKey;
	int mChannel;
};


class SearchResultModel : public MessageListModel
{
	Q_OBJECT
public:

	SearchResultModel(QListView* list);

	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

	virtual void Open(const std::vector<std::shared_ptr<Message>>* m) override;
};

class FoundMessageWidget : public QFrame
{
	Q_OBJECT
public:
	FoundMessageWidget(const Message* m, QWidget* mw);

signals:

	void clicked();

private:

	virtual void mousePressEvent(QMouseEvent* event) override;
};

class SearchResultDelegate : public MessageDelegate
{
	Q_OBJECT
public:

	using MessageDelegate::MessageDelegate;

	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

	int PaintChannel(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight

	QSize GetChannelSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

};


#endif