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

std::vector<std::shared_ptr<Message>> SearchExactPhrase(int ch, MessageListView* mes, const QString& phrase, SearchMode::Case case_)
{
	std::vector<std::shared_ptr<Message>> res;
	if (!mes->IsConstructed()) mes->Construct(gChannelVector[ch].GetName());
	QTextDocument::FindFlags f = case_ == SearchMode::SENSITIVE ? QTextDocument::FindCaseSensitively : 0;
	for (const auto& m : mes->GetMessages())
	{
		bool b = m->HasTextDocument();
		const QTextDocument* d = m->GetTextDocument();
		QTextCursor c = d->find(phrase, f);
		if (!c.isNull()) res.emplace_back(m);
		if (!b) m->ClearTextDocument();//異なるスレッドでQObjectを共有することはできないので、この関数内で新たに作った場合は削除必須。
		if (m->IsParentMessage())
		{
			for (const auto& r : m->GetThread()->GetReplies())
			{
				bool b = r->HasTextDocument();
				const QTextDocument* d = r->GetTextDocument();
				QTextCursor c = d->find(phrase, f);
				if (!c.isNull()) res.emplace_back(r);
				if (!b) r->ClearTextDocument();
			}
		}
	}
	return res;
}
std::vector<std::shared_ptr<Message>> SearchWords(int ch, MessageListView* mes, const QStringList& keys, SearchMode::Match match, SearchMode::Case case_)
{
	std::vector<std::shared_ptr<Message>> res;
	if (!mes->IsConstructed()) mes->Construct(gChannelVector[ch].GetName());
	QTextDocument::FindFlags f = case_ == SearchMode::SENSITIVE ? QTextDocument::FindCaseSensitively : 0;
	for (const auto& m : mes->GetMessages())
	{
		bool b = m->HasTextDocument();
		const QTextDocument* d = m->GetTextDocument();
		bool found = (match == SearchMode::ALLWORDS ? true : false);
		for (const auto& k : keys)
		{
			QTextCursor c = d->find(k, 0, f);
			if (match == SearchMode::ALLWORDS)
			{
				//一つでも含まれなかった場合は見つからなかった扱い。
				if (c.isNull())
				{
					found = false;
					break;
				}
			}
			else
			{
				//一つでも含まれればOK。
				if (!c.isNull())
				{
					found = true;
					break;
				}
			}
		}
		if (found) 
			res.emplace_back(m);
		if (!b) m->ClearTextDocument();//異なるスレッドでQObjectを共有することはできないので、この関数内で新たに作った場合は削除必須。
		if (m->IsParentMessage())
		{
			for (const auto& r : m->GetThread()->GetReplies())
			{
				bool b = r->HasTextDocument();
				const QTextDocument* d = r->GetTextDocument();
				bool found = (match == SearchMode::ALLWORDS ? true : false);
				for (const auto& k : keys)
				{
					QTextCursor c = d->find(k, 0, f);
					if (match == SearchMode::ALLWORDS)
					{
						//一つでも含まれなかった場合は見つからなかった扱い。
						if (c.isNull())
						{
							found = false;
							break;
						}
					}
					else
					{
						//一つでも含まれればOK。
						if (!c.isNull())
						{
							found = true;
							break;
						}
					}
				}
				if (found) res.emplace_back(r);
				if (!b) r->ClearTextDocument();//異なるスレッドでQObjectを共有することはできないので、この関数内で新たに作った場合は削除必須。
			}
		}
	}
	return res;
}
std::vector<std::shared_ptr<Message>> SearchWithRegex(int ch, MessageListView* mes, const QRegularExpression& regex)
{
	std::vector<std::shared_ptr<Message>> res;
	if (!mes->IsConstructed()) mes->Construct(gChannelVector[ch].GetName());
	for (const auto& m : mes->GetMessages())
	{
		bool b = m->HasTextDocument();
		const QTextDocument* d = m->GetTextDocument();
		QTextCursor c = d->find(regex);
		if (!c.isNull()) res.emplace_back(m);
		if (!b) m->ClearTextDocument();//異なるスレッドでQObjectを共有することはできないので、この関数内で新たに作った場合は削除必須。
		if (m->IsParentMessage())
		{
			for (const auto& r : m->GetThread()->GetReplies())
			{
				bool b = r->HasTextDocument();
				const QTextDocument* d = r->GetTextDocument();
				QTextCursor c = d->find(regex);
				if (!c.isNull()) res.emplace_back(r);
				if (!b) r->ClearTextDocument();
			}
		}
	}
	return res;
}

SearchResultListView::SearchResultListView()
	: mChannel(-1)
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
		QPixmap icon("Resources\\batsu.png");
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

size_t SearchResultListView::Search(int ch, const QStackedWidget* stack, const QString& key, SearchMode mode)
{
	//検索キーとmodeが完全一致する場合、すでに結果は出ているので実行しなくても良い。
	if (ch == mChannel && key == mKey && mode == mMode)
		return mView->GetMessages().size();
	mChannel = ch;
	mKey = key;
	mMode = mode;

	SearchResultModel* model = static_cast<SearchResultModel*>(mView->model());
	int c = model->rowCount();
	if (c > 0) model->removeRows(0, c);
	mLabel->setText("Search results for \"" + (mKey.size() > 40 ? mKey.mid(0, 36) + "..." : mKey) + "\"");

	//検索範囲
	std::vector<int> chs;
	if (mode.GetRangeMode() == SearchMode::CURRENTCH)
	{
		//現在のチャンネルのみを検索
		chs = { ch };
	}
	else if (mode.GetRangeMode() == SearchMode::ALLCHS)
	{
		//全チャンネルを検索
		chs.resize(gChannelVector.size());
		for (int i = 0; i < gChannelVector.size(); ++i) chs[i] = i;
	}

	std::vector<std::shared_ptr<Message>> res;
	QList<QFuture<std::vector<std::shared_ptr<Message>>>> fs;
	if (mode.GetRegexMode() == SearchMode::REGEX)
	{
		QRegularExpression regex(key);
		for (auto i : chs)
		{
			MessageListView* mes = static_cast<MessageListView*>(stack->widget(i));
			fs.append(QtConcurrent::run(SearchWithRegex, i, mes, regex));
		}
	}
	else if (mode.GetMatchMode() == SearchMode::EXACTPHRASE)
	{
		for (auto i : chs)
		{
			MessageListView* mes = static_cast<MessageListView*>(stack->widget(i));
			fs.append(QtConcurrent::run(SearchExactPhrase, i, mes, key, mode.GetCaseMode()));
		}
	}
	else
	{
		for (auto i : chs)
		{
			QStringList keys = key.split(QRegularExpression("\\s"));
			MessageListView* mes = static_cast<MessageListView*>(stack->widget(i));
			fs.append(QtConcurrent::run(SearchWords, i, mes, keys, mode.GetMatchMode(), mode.GetCaseMode()));
		}
	}
	for (auto& f : fs)
	{
		f.waitForFinished();
		auto& r = f.result();
		res.insert(res.end(), make_move_iterator(r.begin()), make_move_iterator(r.end()));
	}
	if (chs.size() > 1)
		std::sort(std::execution::par, res.begin(), res.end(),
				  [](const std::shared_ptr<Message>& m1, const std::shared_ptr<Message>& m2)
				  {
					  return m1->GetTimeStamp() < m2->GetTimeStamp();
				  });
	mView->SetMessages(std::move(res));
	return mView->GetMessages().size();
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

FoundMessageWidget::FoundMessageWidget(const Message* m, QWidget* mw)
{
	setStyleSheet("background-color: rgb(239, 239, 239); border: 0px;");
	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(gLeftMargin, gTopMargin, gRightMargin, gBottomMargin);
	layout->setSpacing(0);
	{
		QHBoxLayout* olayout = new QHBoxLayout();
		olayout->setContentsMargins(0, 0, 0, 0);
		//channel name
		QLabel* ch = new QLabel("# " + gChannelVector[m->GetChannel()].GetName());
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
void FoundMessageWidget::mousePressEvent(QMouseEvent* event)
{
	emit clicked();
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
	
	QWidget* message = CreateMessageWidget(*m,
										   GetNameSize(option, index), GetDateTimeSize(option, index),
										   GetTextSize(option, index), GetThreadSize(option, index), parent->width(), m->GetThread() != nullptr);
	message->layout()->setContentsMargins(0, 0, 0, 0);

	FoundMessageWidget* w = new FoundMessageWidget(m, message);
	connect(w, &FoundMessageWidget::clicked, [m]()
			{
				auto* mw = MainWindow::Get();
				int row = m->GetRow();
				if (m->IsReply())
					mw->SetChannelAndIndex(m->GetChannel(), row, m->GetThread()->GetParent()->GetRow());
				else
					mw->SetChannelAndIndex(m->GetChannel(), row);
			});
	w->setParent(parent);
	return w;
}

int SearchResultDelegate::PaintChannel(QPainter* painter, QRect crect, int ypos, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	const QString& ch = gChannelVector[m->GetChannel()].GetName();

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

QSize SearchResultDelegate::GetChannelSize(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	Message* m = static_cast<Message*>(index.internalPointer());
	const QString& name = m->GetUser().GetName();
	const QString& ch = gChannelVector[m->GetChannel()].GetName();
	QFont f;
	f.setBold(true);
	f.setPointSizeF(GetNamePointSize());
	return QFontMetrics(f).boundingRect("# " + ch).size();
}