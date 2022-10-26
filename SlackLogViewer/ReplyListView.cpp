#include <QMouseEvent>
#include <QPainter>
#include <QJsonDocument>
#include "ReplyListView.h"
#include "GlobalVariables.h"
#include <QScrollBar>

bool ReplyListView::ScrollToRow(int row)
{
	ReplyListModel* model = static_cast<ReplyListModel*>(this->model());
	if (row >= model->GetTrueRowCount()) return false;
	while (model->rowCount() < row)
	{
		model->fetchMore(QModelIndex());
	}
	scrollTo(model->index(row, 0));
	return true;
}
void ReplyListView::Close()
{
	static_cast<ReplyListModel*>(model())->Close();
}

ReplyListModel::ReplyListModel(QListView* list)
	: MessageListModel(list), mParentMessage(nullptr)
{}
QVariant ReplyListModel::data(const QModelIndex & index, int role) const
{
	if (role != Qt::DisplayRole) return QVariant();
	if (index.row() == 1) return QVariant();
	const Message* ch = static_cast<const Message*>(index.internalPointer());
	return ch->GetMessage();
}
int ReplyListModel::rowCount(const QModelIndex& parent) const
{
	if (mMessages == nullptr) return 0;
	if (!parent.isValid()) return mSize;
	return 0;
}
QModelIndex ReplyListModel::index(int row, int, const QModelIndex& parent) const
{
	if (parent.isValid()) return QModelIndex();
	if (mParentMessage == nullptr) return QModelIndex();
	if (mMessages == nullptr) return QModelIndex();
	if (row >= mSize) return QModelIndex();
	if (row == 0) return createIndex(row, 0, (void*)mParentMessage);
	if (row == 1) return createIndex(row, 0, (void*)mMessages);
	return createIndex(row, 0, (void*)(*mMessages)[(size_t)row - 2].get());
}
/*bool ReplyListModel::insertRows(int row, int count, const QModelIndex& parent)
{
	beginInsertRows(parent, row, row + count - 1);
	mSize += count;
	endInsertRows();
	return true;
}
bool ReplyListModel::removeRows(int row, int count, const QModelIndex& parent)
{
	beginRemoveRows(parent, row, row + count - 1);
	mSize -= count;
	endRemoveRows();
	return true;
}*/
bool ReplyListModel::canFetchMore(const QModelIndex& parent) const
{
	if (parent.isValid()) return false;
	if (mParentMessage == nullptr) return false;
	if (mMessages == nullptr) return false;
	return mSize == 0 || mSize <= mMessages->size();
}
void ReplyListModel::fetchMore(const QModelIndex& parent)
{
	if (parent.isValid()) return;
	int fetchsize = std::min(gSettings->value("NumOfMessagesPerPage").toInt(), (int)(mMessages->size() - mSize + 2));
	insertRows(mSize, fetchsize);
}
void ReplyListModel::Open(const Message* parent, const std::vector<std::shared_ptr<Message>>* replies)
{
	int c = rowCount();
	if (c > 0) removeRows(0, c);
	mParentMessage = parent;
	mMessages = replies;
	int size = (int)(mMessages->size());
	insertRows(0, size + 2);
}
void ReplyListModel::Close()
{
	int c = rowCount();
	if (c > 0) removeRows(0, c);
	mParentMessage = nullptr;
	mMessages = nullptr;
}
int ReplyListModel::GetTrueRowCount() const
{
	return (int)mMessages->size() + 2;
}

QSize ReplyDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt(option);
	initStyleOption(&opt, index);

	int width = opt.rect.width();

	if (index.row() == 1) return QSize(width, GetBorderSize(opt, index).height() + gSpacing);

	QSize mssize = GetMessageSize(opt, index);
	int height = mssize.height();

	QSize dcsize = GetDocumentSize(opt, index);
	if (dcsize != QSize(0, 0)) height += dcsize.height() + gSpacing;

	QSize rcsize = GetReactionSize(opt, index);
	if (rcsize != QSize(0, 0)) height += rcsize.height() + gSpacing;

	height = height > gIconSize ? height : gIconSize;
	return QSize(width, gTopMargin + gBottomMargin + height);
}
void ReplyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt(option);
	initStyleOption(&opt, index);
	const QRect& rect(option.rect);
	const QRect& crect(rect.adjusted(gLeftMargin, gTopMargin, -gRightMargin, -gBottomMargin));
	painter->save();
	if (index.row() == 1)
	{
		PaintBorder(painter, crect, 0, opt, index);
	}
	else
	{
		painter->setRenderHint(QPainter::Antialiasing, true);
		painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
		painter->setRenderHint(QPainter::TextAntialiasing, true);
		int y = PaintMessage(painter, crect, 0, opt, index);
		y = PaintDocument(painter, crect, y, option, index);
		y = PaintReaction(painter, crect, y, opt, index);
	}
	painter->restore();
}
QWidget* ReplyDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	//MessageDelegateの場合と違い、
	//1. row == 1のときはEditorを生成しない(親メッセージとリプライとのボーダーなので)
	//2. 親メッセージに対してもthreadボタンは表示しない(スレッドを持たないものとして扱う)
	//という2点が異なる。
	if (!index.isValid()) return nullptr;
	if (index.row() == 1) return nullptr;
	Message* m = static_cast<Message*>(index.internalPointer());
	auto* w = new MessageEditor(mListView, *m,
								GetNameSize(option, index), GetDateTimeSize(option, index),
								GetTextSize(option, index), GetThreadSize(option, index), parent->width(), false);
	connect(w, &MessageEditor::copyAvailable, mListView, &MessageListView::UpdateSelection);
	w->setParent(parent);
	w->setStyleSheet("QWidget { border-left: 0px; }");
	return w;
}
QSize ReplyDelegate::GetBorderSize(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
	QFont f;
	f.setBold(true);
	f.setPointSizeF(GetNamePointSize());
	auto* m = static_cast<std::vector<std::shared_ptr<Message>>*>(index.internalPointer());
	return QFontMetrics(f).boundingRect(QString::number(m->size()) + " replies ").size();
}
int ReplyDelegate::PaintBorder(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	auto* m = static_cast<std::vector<std::shared_ptr<Message>>*>(index.internalPointer());
	painter->save();
	QSize size = GetBorderSize(option, index);
	QRect rect(crect.left(), crect.top(), crect.width(), size.height());
	rect.translate(0, ypos);
	QFont f;
	f.setPointSizeF(GetNamePointSize());
	painter->setPen(QColor(64, 64, 64));
	painter->drawText(rect, Qt::TextSingleLine, QString::number(m->size()) + " replies ");
	rect.adjust(size.width(), 0, 0, 0);
	int y = (rect.top() + rect.bottom()) / 2;
	painter->setPen(QColor(192, 192, 192));
	painter->drawLine(rect.left(), y, rect.right(), y);
	painter->restore();
	return ypos + gSpacing;
}