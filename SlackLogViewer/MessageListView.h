#ifndef MESSAGELISTWIDGET_H
#define MESSAGELISTWIDGET_H

#include <QListView>
#include <QAbstractItemModel>
#include <QTextDocument>
#include <QStyledItemDelegate>
#include <QLabel>
#include <QThreadPool>
#include <QTextBrowser>
#include <optional>
#include "Message.h"

class ImageFile;
class TextFile;
class OtherFile;

class QLabel;

class ClickableLabel : public QLabel
{
	Q_OBJECT
public:
	ClickableLabel()
	{
		setCursor(Qt::PointingHandCursor);
	}
Q_SIGNALS:
	void clicked();
private:
	virtual void mousePressEvent(QMouseEvent*) override { Q_EMIT clicked(); }
};

class ImageWidget : public ClickableLabel
{
	Q_OBJECT
public:
	ImageWidget(const ImageFile* i, int pwidth, bool isquote = false);

	const ImageFile* GetImage() const { return mImage; }

private:

	const ImageFile* mImage;

};

class DocumentWidget : public QFrame
{
	Q_OBJECT
public:

	DocumentWidget(const AttachedFile* file, int pwidth);

Q_SIGNALS:

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

Q_SIGNALS:

	void clicked(const Message*);

private:

	virtual void mousePressEvent(QMouseEvent* event) override;
#if QT_VERSION_MAJOR==5
	virtual void enterEvent(QEvent* evt) override;
#elif QT_VERSION_MAJOR==6
	virtual void enterEvent(QEnterEvent* evt) override;
#endif
	void leaveEvent(QEvent* evt) override;

	QLabel* mViewMessage;
	const Message* mParentMessage;
};

class MessageListModel;
class MessageDelegate;

class MessageListView : public QListView
{
	Q_OBJECT
public:

	friend class MessageListModel;
	friend class MessageDelegate;

	MessageListView();

	bool Construct(Channel::Type type, int index);
private:
	//Qt5のQtConcurrent::runは可変長引数になっておらず、ここの関数の引数は5個以下でないと動かない。
	//よって、typeとch_indexを一つにまとめておく。
	static std::pair<std::vector<std::shared_ptr<Message>>, std::vector<std::shared_ptr<Message>>>
		Construct_parallel(const QString& filename, std::pair<Channel::Type, int> ch_type_index, QByteArray data,
						   std::map<QString, std::shared_ptr<Thread>>* threads, std::mutex* thmtx);
public:
	bool IsConstructed() const { return mConstructed; }

	//Clear関数は生成されたQTextDocumentと付随するImageFileを削除しメモリを解放する。
	void Clear();
	//Close関数はメッセージなども含め丸ごと初期化する。
	void Close();

	void UpdateCurrentIndex(QPoint pos);
	void CloseEditorAtSelectedIndex(QPoint pos);
	bool IsSelectedIndex(QPoint pos);

	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void wheelEvent(QWheelEvent* event) override;
	virtual void leaveEvent(QEvent* event) override;

	void SetMessages(std::vector<std::shared_ptr<Message>>&& m) { mMessages = std::move(m); }
	const std::vector<std::shared_ptr<Message>>& GetMessages() const { return mMessages; }

	int GetNumOfPages() const { return (int)mMessages.size() / gSettings->value("NumOfMessagesPerPage").toInt() + 1; }
	int GetCurrentRow();
	int GetCurrentPage();

public Q_SLOTS:

	bool ScrollToRow(int row);
	bool JumpToPage(int page);
	void UpdateCurrentPage();
	void UpdateSelection(bool b);

Q_SIGNALS:

	void CurrentPageChanged(int);

protected:

	QModelIndex mCurrentIndex;
	QModelIndex mSelectedIndex;
	int mPreviousPage;
	bool mConstructed;
	//mMessagesはstd::shared_ptrが望ましい。
	//SearchResultListViewはMessageListViewを継承しているが、
	//この内部ではmMessagesを部分的に共有する必要がある。
	//愚策だが生ポインタを管理するよりはマシだろう。
	std::vector<std::shared_ptr<Message>> mMessages;
	std::map<QString, std::shared_ptr<Thread>> mThreads;//thread_tsをキーとしている。
};

class FileDownloader;

class MessageListModel : public QAbstractListModel
{
	Q_OBJECT
public:

	MessageListModel(QListView* list);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	virtual bool canFetchMore(const QModelIndex& parent) const override;
	virtual void fetchMore(const QModelIndex& parent) override;

	virtual int GetTrueRowCount() const;

public Q_SLOTS:

	virtual void Open(const std::vector<std::shared_ptr<Message>>* m);
	void Close();

Q_SIGNALS:

	//本当は引数は最初の一つだけで十分だが、dataChangedに繋ぐのでそちらの分だけ用意しておく。
	void ImageDownloadFinished(const QModelIndex& index1, const QModelIndex& index2, const QVector<int>&);

protected:

	QListView* mListView;
	const std::vector<std::shared_ptr<Message>>* mMessages;
	int mSize;
};

class SlackLogViewer;

//QWidget* CreateMessageWidget(Message& m, QSize namesize, QSize datetimesize, QSize textsize, QSize threadsize, int pwidth, bool has_thread);

class MessageBrowser : public QTextBrowser
{
	virtual void mousePressEvent(QMouseEvent* event) override;
};

class MessageEditor : public QWidget
{
	//別にEditするものではないが、命名規則上。
	//基底クラスをQFrameにすると、左側にだけボーダーラインのあるReplyListView上で表示したときに微妙に表示位置がずれる。
	//なので、QWidgetを基底クラスとして、背景色が塗られない点はpaintEventをオーバーライドすることで対応する。
	Q_OBJECT
public:

	MessageEditor(MessageListView* view, Message& m,
				  QSize namesize, QSize datetimesize, QSize textsize, QSize threadsize, int pwidth, bool has_thread);

	MessageListView* GetMessageListView() { return mListView; }

Q_SIGNALS:

	void copyAvailable(bool b);

public Q_SLOTS:

#if QT_VERSION_MAJOR==5
	virtual void enterEvent(QEvent* evt) override;
#elif QT_VERSION_MAJOR==6
	virtual void enterEvent(QEnterEvent* evt) override;
#endif
	virtual void leaveEvent(QEvent* evt) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void mouseMoveEvent(QMouseEvent* event) override;
	virtual void paintEvent(QPaintEvent* event) override;

private:

	MessageListView* mListView;

};

class MessageDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:

	MessageDelegate(MessageListView* list);

	QListView* GetListView() { return mListView; }
	const QListView* GetListView() const { return mListView; }

	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
	virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

	int PaintMessage(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight
	int PaintQuote(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight
	int PaintReaction(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight
	int PaintThread(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight
	int PaintDocument(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;//戻り値はypos + 消費したheight

	int PaintSeparator(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QSize GetMessageSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QSize GetNameSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	QSize GetDateTimeSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	QSize GetTextSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QSize GetQuoteSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	QSize GetQuoteTextSize(const QStyleOptionViewItem& option, const QModelIndex& index, int n) const;

	QSize GetReactionSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QSize GetThreadSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;
	QSize GetDocumentSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

	QSize GetSeparatorSize(const QStyleOptionViewItem& option, const QModelIndex& index) const;

protected:

	MessageListView* mListView;
};

#endif