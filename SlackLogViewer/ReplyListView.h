#ifndef REPLYLISTVIEW_H
#define REPLYLISTVIEW_H

#include "MessageListView.h"

class ReplyListView : public MessageListView
{
	Q_OBJECT
public:

	void Close();

public Q_SLOTS:

	bool ScrollToRow(int row);

};

class ReplyListModel : public MessageListModel
{
	Q_OBJECT
public:

	ReplyListModel(QListView* list);
	virtual QVariant data(const QModelIndex& index, int role) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	//virtual bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	//virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	virtual bool canFetchMore(const QModelIndex& parent) const override;
	virtual void fetchMore(const QModelIndex& parent) override;

	virtual int GetTrueRowCount() const;

	bool IsEmpty() const { return mParentMessage == nullptr; }
	int GetChannelIndex() const { return mParentMessage->GetChannel(); }

public Q_SLOTS:

	void Open(const Message* parent, const std::vector<std::shared_ptr<Message>>* replies);
	void Close();

private:

	const Message* mParentMessage;
};

class ReplyDelegate : public MessageDelegate
{
	Q_OBJECT
public:

	using MessageDelegate::MessageDelegate;

	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
protected:

	QSize GetBorderSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	int PaintBorder(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;
};
#endif