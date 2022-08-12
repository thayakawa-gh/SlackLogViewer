#ifndef SEARCHRESULTVIEW_H
#define SEARCHRESULTVIEW_H

#include "MessageListView.h"

struct SearchMode
{
	enum Match : char { EXACTPHRASE, ALLWORDS, ANYWORDS, };
	enum Case : char { INSENSITIVE, SENSITIVE, };
	enum Regex : char { KEYWORDS, REGEX, };
	enum Range : char { ALLCHS, VISIBLECHS, CURRENTCH, };

	SearchMode(Match match, Case case_, Regex regex, Range range, bool body, bool user, bool fname, bool fcont)
		: mValue(0)
	{
		mValue |= (char)match;
		mValue |= (char)case_ << 2;
		mValue |= (char)regex << 4;
		mValue |= (char)range << 6;
		mValue |= (char)body << 8;
		mValue |= (char)user << 9;
		mValue |= (char)fname << 10;
		mValue |= (char)fcont << 11;
	}
	SearchMode()
		: SearchMode(EXACTPHRASE, INSENSITIVE, KEYWORDS, CURRENTCH, false, false, false, false)
	{}

	bool operator==(SearchMode mode) const { return mValue == mode.mValue; }
	bool operator!=(SearchMode mode) const { return !(*this == mode); }

	Match GetMatchMode() const { return (Match)(mValue & 0b11); }
	Case GetCaseMode() const { return (Case)((mValue & 0b1100) >> 2); }
	Regex GetRegexMode() const { return (Regex)((mValue & 0b110000) >> 4); }
	Range GetRangeMode() const { return (Range)((mValue & 0b11000000) >> 6); }

	bool Body() const { return mValue & 0b100000000; }
	bool User() const { return mValue & 0b1000000000; }
	bool FileName() const { return mValue & 0b10000000000; }
	bool FileContents() const { return mValue & 0b100000000000; }

private:

	short mValue;
};

std::vector<std::shared_ptr<Message>> SearchExactPhrase(int ch, MessageListView* mes, QString phrase, SearchMode mode);
std::vector<std::shared_ptr<Message>> SearchWords(int ch, MessageListView* mes, QStringList keys, SearchMode mode);
std::vector<std::shared_ptr<Message>> SearchWithRegex(int ch, MessageListView* mes, QRegularExpression regex, SearchMode mode);

class QStackedWidget;

class SearchResultListView : public QWidget
{
	Q_OBJECT
public:

	SearchResultListView();

	size_t Search(int ch, const QStackedWidget* stack, const QString& key, SearchMode mode);
	void Close();

	const std::vector<std::shared_ptr<Message>>& GetMessages() const { return mView->GetMessages(); }

	MessageListView* GetView() { return mView; }

Q_SIGNALS:

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

class FoundMessageEditorInside : public MessageEditor
{
	using MessageEditor::MessageEditor;
	//enter、leaveEventをオーバーライドして、背景色を変更する処理を消す。
	virtual void enterEvent(QEvent* evt) override;
	virtual void leaveEvent(QEvent* evt) override;
};
class FoundMessageEditor : public QFrame
{
	Q_OBJECT
public:

	FoundMessageEditor(const Message* m, QWidget* mw);

	virtual void enterEvent(QEvent* evt) override;
	void leaveEvent(QEvent* evt) override;

Q_SIGNALS:

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