#include "SearchResultList.h"
#include "GlobalVariables.h"
#include "SlackLogViewer.h"
#include <QScrollBar>
#include <QMouseEvent>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QStackedWidget>
#include <QtConcurrent>
#include <execution>

std::vector<std::shared_ptr<Message>> SearchExactPhrase(Channel::Type /*ch_type*/, int /*ch*/, MessageListView* mes, QString phrase, SearchMode mode)
{
	std::vector<std::shared_ptr<Message>> res;
	bool case_sensitive = mode.GetCaseMode() == SearchMode::SENSITIVE ? true : false;
	bool body = mode.Body();
	bool user = mode.User();
	bool fname = mode.FileName();
	bool fcont = mode.FileContents();
	for (const auto& m : mes->GetMessages())
	{
		bool found = m->Contains(phrase, case_sensitive, body, user, fname, fcont);
		if (found) res.emplace_back(m);
		if (m->IsParentMessage())
		{
			for (const auto& r : m->GetThread()->GetReplies())
			{
				bool found2 = r->Contains(phrase, case_sensitive, body, user, fname, fcont);
				if (found2) res.emplace_back(r);
			}
		}
	}
	return res;
}
std::vector<std::shared_ptr<Message>> SearchWords(Channel::Type /*ch_type*/, int /*ch*/, MessageListView* mes, QStringList keys, SearchMode mode)
{
	std::vector<std::shared_ptr<Message>> res;
	bool case_sensitive = mode.GetCaseMode() == SearchMode::SENSITIVE ? true : false;
	bool all = (mode.GetMatchMode() == SearchMode::ALLWORDS ? true : false);
	bool body = mode.Body();
	bool user = mode.User();
	bool fname = mode.FileName();
	bool fcont = mode.FileContents();
	for (const auto& m : mes->GetMessages())
	{
		bool found = m->Contains(keys, case_sensitive, all, body, user, fname, fcont);
		if (found) 
			res.emplace_back(m);
		if (m->IsParentMessage())
		{
			for (const auto& r : m->GetThread()->GetReplies())
			{
				bool found2 = r->Contains(keys, case_sensitive, all, body, user, fname, fcont);
				if (found2) res.emplace_back(r);
			}
		}
	}
	return res;
}
std::vector<std::shared_ptr<Message>> SearchWithRegex(Channel::Type /*ch_type*/, int /*ch*/, MessageListView* mes, QRegularExpression regex, SearchMode mode)
{
	std::vector<std::shared_ptr<Message>> res;
	bool body = mode.Body();
	bool user = mode.User();
	bool fname = mode.FileName();
	bool fcont = mode.FileContents();
	for (const auto& m : mes->GetMessages())
	{
		bool found = m->Contains(regex, body, user, fname, fcont);
		if (found) res.emplace_back(m);
		if (m->IsParentMessage())
		{
			for (const auto& r : m->GetThread()->GetReplies())
			{
				bool found2 = r->Contains(regex, body, user, fname, fcont);
				if (found2) res.emplace_back(r);
			}
		}
	}
	return res;
}

SearchResultListView::SearchResultListView()
	: mChannel(-1), mChType(Channel::END_CH)
{
	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(gSpacing, gSpacing, gSpacing, gSpacing);
	{
		//bar
		QHBoxLayout* bar = new QHBoxLayout();
		mLabel = new QLabel("Search results");
		QFont f;
		f.setBold(true);
		mLabel->setFont(f);
		bar->addWidget(mLabel);

		bar->addStretch();

		QPushButton* close = new QPushButton();
		close->setCursor(Qt::PointingHandCursor);
		close->setStyleSheet("border: 0px;");
		QPixmap icon(ResourcePath("batsu.png"));
		close->setIcon(icon);
		connect(close, &QPushButton::clicked, this, &SearchResultListView::Closed);
		bar->addWidget(close);
		layout->addLayout(bar);
	}
	{
		//listview
		mView = new MessageListView();
		mView->setMouseTracking(true);
		mView->setVerticalScrollMode(QListView::ScrollPerPixel);
		mView->verticalScrollBar()->setSingleStep(gSettings->value("ScrollStep").toInt());
		mView->setStyleSheet("border: 0px;");
		layout->addWidget(mView);
	}
	setLayout(layout);
}

std::pair<size_t, bool> SearchResultListView::Search(Channel::Type ch_type, int ch, const QStackedWidget* stack, const QString& key, SearchMode mode)
{
	//検索キーとmodeが完全一致する場合、すでに結果は出ているので実行しなくても良い。
	if (ch == mChannel && ch_type == mChType && key == mKey && mode == mMode)
		return { mView->GetMessages().size(), false };
	mChannel = ch;
	mChType = ch_type;
	mKey = key;
	mMode = mode;

	SearchResultModel* model = static_cast<SearchResultModel*>(mView->model());
	int c = model->rowCount();
	if (c > 0) model->removeRows(0, c);
	mLabel->setText("Search results for \"" + (mKey.size() > 40 ? mKey.mid(0, 36) + "..." : mKey) + "\"");

	//検索範囲
	std::vector<std::pair<Channel::Type, int>> chs;
	if (mode.GetRangeMode() == SearchMode::CURRENTCH)
	{
		//現在のチャンネルのみを検索
		chs = { { ch_type, ch } };
	}
	else if (mode.GetRangeMode() == SearchMode::ALLCHS)
	{
		//全チャンネルを検索
		chs.resize(gChannelVector.size() + gDMUserVector.size() + gGMUserVector.size());
		int i = 0;
		for (int j = 0; j < gChannelVector.size(); ++j) { chs[i] = { Channel::CHANNEL, j }; ++i; };
		for (int j = 0; j < gDMUserVector.size(); ++j) { chs[i] = { Channel::DIRECT_MESSAGE, j }; ++i; };
		for (int j = 0; j < gGMUserVector.size(); ++j) { chs[i] = { Channel::GROUP_MESSAGE, j }; ++i; };
	}

	std::vector<std::shared_ptr<Message>> res;
	QList<QFuture<std::vector<std::shared_ptr<Message>>>> fs;
	for (auto [ ch_type_, ch_index_ ] : chs)
	{
		//まだチャンネルが読み込まれていない場合、ここで読み込んでおく。
		//チャンネルの読み込み、検索はどちらもマルチスレッドに行われるので、一応分離しておく。
		int row = IndexToRow(ch_type_, ch_index_);
		MessageListView* mes = static_cast<MessageListView*>(stack->widget(row));
		if (!mes->IsConstructed()) mes->Construct(ch_type_, ch_index_);
	}
	if (mode.GetRegexMode() == SearchMode::REGEX)
	{
		QRegularExpression regex(key);
		for (auto [ch_type_, ch_index_] : chs)
		{
			int row = IndexToRow(ch_type_, ch_index_);
			MessageListView* mes = static_cast<MessageListView*>(stack->widget(row));
			fs.append(QtConcurrent::run(SearchWithRegex, ch_type_, ch_index_, mes, regex, mode));
		}
	}
	else if (mode.GetMatchMode() == SearchMode::EXACTPHRASE)
	{
		for (auto [ch_type_, ch_index_] : chs)
		{
			int row = IndexToRow(ch_type_, ch_index_);
			MessageListView* mes = static_cast<MessageListView*>(stack->widget(row));
			fs.append(QtConcurrent::run(SearchExactPhrase, ch_type_, ch_index_, mes, key, mode));
		}
	}
	else
	{
		QStringList keys = key.split(QRegularExpression("\\s"));
		for (auto [ch_type_, ch_index_] : chs)
		{
			int row = IndexToRow(ch_type_, ch_index_);
			MessageListView* mes = static_cast<MessageListView*>(stack->widget(row));
			fs.append(QtConcurrent::run(SearchWords, ch_type_, ch_index_, mes, keys, mode));
		}
	}
	for (auto& f : fs)
	{
		f.waitForFinished();
		auto r = f.result();
		res.insert(res.end(), make_move_iterator(r.begin()), make_move_iterator(r.end()));
	}
	if (chs.size() > 1)
		std::sort(std::execution::par, res.begin(), res.end(),
				  [](const std::shared_ptr<Message>& m1, const std::shared_ptr<Message>& m2)
				  {
					  return m1->GetTimeStamp() < m2->GetTimeStamp();
				  });
	mView->SetMessages(std::move(res));
	return { mView->GetMessages().size(), true };
}
void SearchResultListView::Close()
{
	mView->Close();
	mChannel = -1;
	mKey = "";
	mMode = SearchMode();
	mLabel->setText("");
}

SearchResultModel::SearchResultModel(QListView* list)
	: MessageListModel(list)
{}
bool SearchResultModel::removeRows(int row, int count, const QModelIndex & parent)
{
	beginRemoveRows(parent, row, row + count - 1);
	mSize -= count;
	endRemoveRows();
	return true;
}
void SearchResultModel::Open(const std::vector<std::shared_ptr<Message>>* m)
{
	int c = rowCount();
	if (c > 0) removeRows(0, c);
	MessageListModel::Open(m);
}

void FoundMessageEditorInside::enterEvent(QEvent* evt)
{
	QWidget::enterEvent(evt);
}
void FoundMessageEditorInside::leaveEvent(QEvent* evt)
{
	QWidget::leaveEvent(evt);
}
FoundMessageEditor::FoundMessageEditor(const Message* m, QWidget* mw)
{
	setStyleSheet("background-color: rgb(239, 239, 239); border: 0px;");
	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(gLeftMargin, gTopMargin, gRightMargin, gBottomMargin);
	layout->setSpacing(0);
	{
		QHBoxLayout* olayout = new QHBoxLayout();
		olayout->setContentsMargins(0, 0, 0, 0);
		//channel name
		int row = IndexToRow(m->GetChannelType(), m->GetChannel());
		QLabel* ch = new QLabel("# " + GetChannel(row).GetName());
		QFont f;
		f.setPointSizeF(GetNamePointSize());
		f.setBold(true);
		ch->setFont(f);
		ch->setStyleSheet("color: #195BB2; border: 0px;");
		olayout->addWidget(ch);
		olayout->addSpacing(gSpacing * 3);
		//View in channel
		QLabel* view = new QLabel("View in channel");
		f.setBold(false);
		view->setFont(f);
		view->setStyleSheet("color: rgb(64, 64, 64); border: 0px;");
		olayout->addWidget(view);
		olayout->addStretch();
		layout->addLayout(olayout);
	}
	layout->addWidget(mw);
	setLayout(layout);
	setCursor(Qt::PointingHandCursor);
}
void FoundMessageEditor::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) Q_EMIT clicked();
}
void FoundMessageEditor::enterEvent(QEvent*)
{
	setStyleSheet("background-color: rgb(239, 239, 239);");
	layout()->itemAt(1)->widget()->setStyleSheet("background-color: rgb(239, 239, 239);");
}
void FoundMessageEditor::leaveEvent(QEvent*)
{
	setStyleSheet("background-color: white;");
	layout()->itemAt(1)->widget()->setStyleSheet("background-color: white;");
}

QSize SearchResultDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt(option);
	initStyleOption(&opt, index);

	int width = opt.rect.width();

	QSize chsize = GetChannelSize(opt, index);
	int height = chsize.height();

	QSize mssize = GetMessageSize(opt, index);
	height += mssize.height();

	QSize dcsize = GetDocumentSize(opt, index);
	if (dcsize != QSize(0, 0)) height += dcsize.height() + gSpacing;

	QSize rcsize = GetReactionSize(opt, index);
	if (rcsize != QSize(0, 0)) height += rcsize.height() + gSpacing;

	QSize thsize = GetThreadSize(opt, index);
	if (thsize != QSize(0, 0)) height += thsize.height() + gSpacing;

	height = height > gIconSize ? height : gIconSize;
	return QSize(width, gTopMargin + gBottomMargin + height);
}
void SearchResultDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyleOptionViewItem opt(option);
	initStyleOption(&opt, index);
	const QRect& rect(option.rect);
	const QRect& crect(rect.adjusted(gLeftMargin, gTopMargin, -gRightMargin, -gBottomMargin));
	painter->save();
	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
	painter->setRenderHint(QPainter::TextAntialiasing, true);
	int y = PaintChannel(painter, crect, 0, opt, index);
	y = PaintMessage(painter, crect, y, opt, index);
	y = PaintDocument(painter, crect, y, opt, index);
	y = PaintReaction(painter, crect, y, opt, index);
	y = PaintThread(painter, crect, y, opt, index);
	painter->restore();
}

QWidget* SearchResultDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());

	auto* message = new FoundMessageEditorInside(mListView, *m,
												 GetNameSize(option, index), GetDateTimeSize(option, index),
												 GetTextSize(option, index), GetThreadSize(option, index), parent->width(), m->GetThread() != nullptr);
	message->layout()->setContentsMargins(0, 0, 0, 0);
	connect(message, &FoundMessageEditorInside::copyAvailable, mListView, &MessageListView::UpdateSelection);

	FoundMessageEditor* w = new FoundMessageEditor(m, message);
	connect(w, &FoundMessageEditor::clicked, [m]()
			{
				auto* mw = MainWindow::Get();
				int row = m->GetRow();
				if (m->IsReply())
					mw->SetChannelAndIndex(m->GetChannelType(), m->GetChannel(), row, m->GetThread()->GetParent()->GetRow());
				else
					mw->SetChannelAndIndex(m->GetChannelType(), m->GetChannel(), row);
			});
	w->setParent(parent);
	return w;
}

int SearchResultDelegate::PaintChannel(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	int row = IndexToRow(m->GetChannelType(), m->GetChannel());
	const QString& ch = GetChannel(row).GetName();

	painter->save();

	QSize size = GetChannelSize(option, index);
	QRect rect(crect.left(), crect.top(), size.width(), size.height());
	QFont f(option.font);
	f.setPointSizeF(GetNamePointSize());
	f.setBold(true);
	painter->setPen(QPen(QBrush(QColor(64, 64, 64)), 2));
	painter->setFont(f);
	painter->drawText(rect, Qt::TextSingleLine, "# " + ch);

	painter->restore();
	return ypos + size.height();
}

QSize SearchResultDelegate::GetChannelSize(const QStyleOptionViewItem& /*option*/, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	int row = IndexToRow(m->GetChannelType(), m->GetChannel());
	const QString& ch = GetChannel(row).GetName();
	QFont f;
	f.setBold(true);
	f.setPointSizeF(GetNamePointSize());
	return QFontMetrics(f).boundingRect("# " + ch).size();
}
