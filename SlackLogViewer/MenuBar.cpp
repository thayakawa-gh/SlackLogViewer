#include "MenuBar.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QIcon>
#include <QAction>
#include <QTreeWidget>
#include <QHeaderView>
#include <QCompleter>
#include <QRegularExpression>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QStandardItemModel>
#include <QCalendarWidget>
#include <QDateEdit>
#include "CacheStatus.h"

QVariant InnerSearchBox::inputMethodQuery(Qt::InputMethodQuery query) const
{
	QVariant x = QLineEdit::inputMethodQuery(query);
	if (query == Qt::ImCursorRectangle)
	{
		QPoint p = mapToGlobal(QPoint(0, 0));
		QRect r = x.toRect();
		//printf("SearchBox::inputMethodQuery1 (x, y, wx, wy) = (%d, %d, %d, %d)\n", r.x(), r.y(), r.width(), r.height());
		//r.translate(p.x(), 0);
		//printf("SearchBox::inputMethodQuery2 (x, y, wx, wy) = (%d, %d, %d, %d)\n", r.x(), r.y(), r.width(), r.height());
		x = r;
	}
	return x;
}

/*CalendarPopup::CalendarPopup()
{
	QVBoxLayout* layout = new QVBoxLayout();
	{
		//dateedit
		QHBoxLayout* rlayout = new QHBoxLayout();
		mBegin = new QDateEdit();
		mBegin->setCalendarPopup(true);
		rlayout->addWidget(mBegin);
		mEnd = new QDateEdit();
		mEnd->setCalendarPopup(true);
		rlayout->addWidget(mEnd);
		layout->addLayout(rlayout);
	}
	mCalendar = new QCalendarWidget();
	layout->addWidget(mCalendar);
	this->setLayout(layout);
}*/

SearchBoxPopup::SearchBoxPopup(SearchBox* parent)
	: QWidget(nullptr), mParent(parent)
{
	this->setStyleSheet("QWidget { background-color: rgb(53, 13, 54); border: 0px solid rgb(160, 160, 160); }");
	this->setFixedWidth(mParent->width());
	this->setWindowFlags(Qt::Popup | Qt::WindowStaysOnTopHint);
	//this->setWindowFlags(Qt::ToolTip | Qt::Window);
	//this->installEventFilter(this);
	//this->setAttribute(Qt::WA_ShowWithoutActivating);
	this->setFocusPolicy(Qt::NoFocus);
	this->setMouseTracking(true);

	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	{
		mBox = new InnerSearchBox();
		mBox->setStyleSheet("QLineEdit { color: rgb(255, 255, 255); border: 1px solid rgb(160, 160, 160); border-radius: 5px; }"
							"QMenu { color: black; background-color: white; border: 1px solid grey; border-radius: 0px; }"
							"QMenu::item:disabled { color: grey; }"
							"QMenu::item:selected { background-color: palette(Highlight); }");
		mBox->setClearButtonEnabled(true);
		mBox->setPlaceholderText("Search");
		mBox->setFixedSize(400, 24);
		mBox->setFocusPolicy(Qt::NoFocus);
		layout->addWidget(mBox);

		connect(mBox, SIGNAL(returnPressed()), this, SIGNAL(SearchRequested()));
		//connect(this, &SearchBoxPopup::Hidden, [this]() { mParent->setText(mBox->text()); });
		//connect(this, &SearchBoxPopup::Hidden, [this]() { mParent->setCursorPosition(mBox->cursorPosition()); });
		connect(mBox, &QLineEdit::cursorPositionChanged, [this](int /*oldpos*/, int newpos) { mParent->setCursorPosition(newpos); });
		connect(mBox, &QLineEdit::textChanged, [this](const QString& str)
				{
					mParent->setText(str);
					//printf("textchanged\n");
				});
	}
	{
		QFrame* f = new QFrame();
		f->setStyleSheet("background-color: white; color: black; border: 1px solid grey; border-radius: 0px;");
		QVBoxLayout* olayout = new QVBoxLayout();
		QHBoxLayout* slayout = new QHBoxLayout();
		QHBoxLayout* tlayout = new QHBoxLayout();
		olayout->setContentsMargins(2, 2, 2, 2);
		slayout->setContentsMargins(0, 0, 0, 0);
		tlayout->setContentsMargins(0, 0, 0, 0);
		olayout->addLayout(slayout);
		olayout->addLayout(tlayout);
		f->setLayout(olayout);
		{
			//match
			mMatch = new QComboBox();
			{
				mMatch->addItem("Exact phrase");
				mMatch->addItem("All words");
				mMatch->addItem("Any words");
				mMatch->setFocusPolicy(Qt::NoFocus);
				mMatch->setStyleSheet("border-radius: 0px;");
				mMatch->setCurrentIndex(gSettings->value("Search/Match").toInt());
				slayout->addWidget(mMatch);
			}
			//case
			mCase = new QPushButton();
			{
				mCase->setCheckable(true);
				mCase->setIcon(QIcon("Resources/upper_lower_case.png"));
				mCase->setToolTip("Case sensitive");
				mCase->setFocusPolicy(Qt::NoFocus);
				mCase->setStyleSheet("QPushButton:checked { color: white; background-color: lightblue; } QPushButton { border-radius: 0px; }");
				mCase->setChecked(gSettings->value("Search/Case").toBool());
				slayout->addWidget(mCase);
			}
			//regex
			mRegex = new QPushButton();
			{
				mRegex->setCheckable(true);
				mRegex->setIcon(QIcon("Resources/regex.png"));
				mRegex->setToolTip("Regular expression");
				mRegex->setFocusPolicy(Qt::NoFocus);
				mRegex->setStyleSheet("QPushButton:checked { color: white; background-color: lightblue; } QPushButton { border-radius: 0px; }");
				mRegex->setChecked(gSettings->value("Search/Regex").toBool());
				slayout->addWidget(mRegex);

				auto disabler = [this](bool b)
				{
					mMatch->setDisabled(b);
					auto* model = qobject_cast<QStandardItemModel*>(mMatch->model());
					int nrow = model->rowCount();
					for (int i = 0; i < nrow; ++i)
					{
						model->item(i)->setEnabled(!b);
					}
					mCase->setDisabled(b);
				};
				connect(mRegex, &QPushButton::toggled, disabler);
			}
			//range
			mRange = new QComboBox();
			{
				mRange->addItem("Current channel");
				mRange->addItem("All channels");
				mRange->setFocusPolicy(Qt::NoFocus);
				mRange->setStyleSheet("border-radius: 0px;");
				mRange->setCurrentIndex(gSettings->value("Search/Range").toInt());
				slayout->addWidget(mRange);
			}
			slayout->addStretch();
		}
		layout->addWidget(f);
		{
			//target
			QLabel* label = new QLabel("Filter messages by ");
			label->setStyleSheet("border: 0px;");
			tlayout->addWidget(label);
			mTargets.resize(3);
			const QVariantList& check = gSettings->value("Search/Filter").toList();
			auto t = mTargets.begin();
			auto c = check.begin();
			for (; t != mTargets.end(); ++t)
			{
				*t = new QPushButton();
				(*t)->setCheckable(true);
				(*t)->setChecked(c != check.end() ? c->toBool() : false);
				tlayout->addWidget((*t));
				(*t)->setFocusPolicy(Qt::NoFocus);
				(*t)->setStyleSheet("QPushButton:checked { color: black; background-color: lightblue; } QPushButton { border-radius: 0px; }");
				if (c != check.end()) ++c;
			}
			tlayout->addStretch();
			mTargets[0]->setText("Body");
			mTargets[1]->setText("User");
			mTargets[2]->setText("File name");

			//DateTime
			/*QToolButton* calendar = new QToolButton();
			calendar->setCursor(Qt::PointingHandCursor);
			calendar->setText("Calendar");
			//calendar->setIcon(QIcon("Resources/cache.svg"));
			calendar->setToolTip("Cache");
			calendar->setPopupMode(QToolButton::InstantPopup);
			calendar->setStyleSheet("QToolButton::menu-indicator { image: none; }");
			QWidgetAction* action = new QWidgetAction(this);
			CalendarPopup* popup = new CalendarPopup();
			action->setDefaultWidget(popup);
			calendar->addAction(action);
			tlayout->addWidget(calendar);*/
		}
	}
	setLayout(layout);
}

void SearchBoxPopup::closeEvent(QCloseEvent*)
{
	gSettings->setValue("Search/Match", mMatch->currentIndex());
	gSettings->setValue("Search/Case", mCase->isChecked());
	gSettings->setValue("Search/Regex", mRegex->isChecked());
	gSettings->setValue("Search/Range", mRange->currentIndex());
	QVariantList list;
	for (auto& t : mTargets)
	{
		list.append(t->isChecked());
	}
	gSettings->setValue("Search/Filter", list);
}

void SearchBoxPopup::ShowPopup()
{
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	this->move(mParent->mapToGlobal(QPoint(0, 0)));
	mBox->setText(mParent->text());
	mBox->setCursorPosition(mParent->cursorPosition());
	this->show();
	//this->setFocus();//一時的にFocusする。一度こちらをFocusしておかないと、クリック時のparentのFocusでカーソルが表示されない。
}
void SearchBoxPopup::HidePopup()
{
	this->hide();
}
SearchMode SearchBoxPopup::GetSearchMode() const
{
	SearchMode::Match m = SearchMode::EXACTPHRASE;
	switch (mMatch->currentIndex())
	{
	case 0: m = SearchMode::EXACTPHRASE; break;
	case 1: m = SearchMode::ALLWORDS; break;
	case 2: m = SearchMode::ANYWORDS; break;
	}
	SearchMode::Case c = mCase->isChecked() ? SearchMode::SENSITIVE : SearchMode::INSENSITIVE;
	SearchMode::Regex r = mRegex->isChecked() ? SearchMode::REGEX : SearchMode::KEYWORDS;
	SearchMode::Range n = SearchMode::CURRENTCH;
	switch (mRange->currentIndex())
	{
	case 0: n = SearchMode::CURRENTCH; break;
	case 1: n = SearchMode::ALLCHS; break;
	}
	bool body = mTargets[0]->isChecked();
	bool user = mTargets[1]->isChecked();
	bool fname = mTargets[2]->isChecked();
	//bool fcont = mTargets[3]->isChecked();
	return SearchMode(m, c, r, n, body, user, fname, false);
}

SearchBox::SearchBox(QWidget* parent)
	: QLineEdit(parent)
{
	setStyleSheet("color: rgb(255, 255, 255); border: 1px solid rgb(160, 160, 160); border-radius: 5px;");
	//setClearButtonEnabled(true);//Popupを弄った影響でclearbuttonが上手く動作しないので、無効化しておく。
	installEventFilter(this);
	setPlaceholderText("Search");
	setFixedSize(400, 24);
	mPopup = new SearchBoxPopup(this);
	connect(this, SIGNAL(returnPressed()), this, SLOT(ExecuteSearch()));
	connect(mPopup, &SearchBoxPopup::SearchRequested, this, &SearchBox::ExecuteSearch);
}
void SearchBox::keyPressEvent(QKeyEvent* event)
{
	//return QLineEdit::keyPressEvent(event);
	if (mPopup->isHidden())
	{
		mPopup->ShowPopup();
		mPopup->GetSearchBox()->setFocus();
		mPopup->GetSearchBox()->activateWindow();
	}
	mPopup->PassEventToBox(event);
}
void SearchBox::mousePressEvent(QMouseEvent* event)
{
	//return QLineEdit::mousePressEvent(event);
	if (mPopup->isHidden())
	{
		mPopup->ShowPopup();
		mPopup->GetSearchBox()->setFocus(Qt::MouseFocusReason);
		mPopup->GetSearchBox()->activateWindow();
	}
	mPopup->PassEventToBox(event);
}
void SearchBox::inputMethodEvent(QInputMethodEvent* event)
{
	if (mPopup->isHidden())
	{
		mPopup->ShowPopup();
		mPopup->GetSearchBox()->setFocus();
		mPopup->GetSearchBox()->activateWindow();
	}
	mPopup->PassEventToBox(event);
}

QVariant SearchBox::inputMethodQuery(Qt::InputMethodQuery query) const
{
	return mPopup->GetSearchBox()->inputMethodQuery(query);
}
void SearchBox::ExecuteSearch()
{
	const QString& str = this->text();
	if (str.isEmpty()) return;
	mPopup->HidePopup();
	Q_EMIT SearchRequested(str, mPopup->GetSearchMode());
}


MenuBar::MenuBar(QMenu* menu, QWidget* parent)
	: QFrame(parent)
{
	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(gSpacing, gSpacing, gSpacing, gSpacing);
	setStyleSheet("background-color: rgb(53,13,54); border: 0px solid rgb(160, 160, 160);");
	{
		QToolButton* menubutton = new QToolButton();
		menubutton->setCursor(Qt::PointingHandCursor);
		menubutton->setIcon(QIcon("Resources/menu.png"));
		menubutton->setStyleSheet("QToolButton::menu-indicator { image: none; }");
		menubutton->setPopupMode(QToolButton::InstantPopup);
		menubutton->setMenu(menu);
		layout->addWidget(menubutton);
		layout->addStretch();
	}
	{
		mSearchBox = new SearchBox();
		layout->addWidget(mSearchBox, Qt::AlignCenter);
		connect(mSearchBox, SIGNAL(SearchRequested(QString, SearchMode)), this, SIGNAL(SearchRequested(QString, SearchMode)));
	}
	{
		QPushButton* exe = new QPushButton();
		exe->setIcon(QIcon("Resources/search.png"));
		exe->setCursor(Qt::PointingHandCursor);
		layout->addWidget(exe);
		connect(exe, &QPushButton::clicked, mSearchBox, &SearchBox::ExecuteSearch);
		layout->addStretch();
	}
	{
		QToolButton* cachebutton = new QToolButton();
		cachebutton->setCursor(Qt::PointingHandCursor);
		cachebutton->setIcon(QIcon("Resources/cache.svg"));
		cachebutton->setToolTip("Cache");
		cachebutton->setPopupMode(QToolButton::InstantPopup);
		cachebutton->setStyleSheet("QToolButton::menu-indicator { image: none; }");
		QWidgetAction* action = new QWidgetAction(this);
		CacheStatus* state = new CacheStatus(this);
		action->setDefaultWidget(state);
		cachebutton->addAction(action);
		layout->addWidget(cachebutton);
		connect(state, SIGNAL(CacheAllRequested(CacheStatus::Channel, CacheStatus::Type)),
				this, SIGNAL(CacheAllRequested(CacheStatus::Channel, CacheStatus::Type)));
		connect(state, SIGNAL(ClearCacheRequested(CacheStatus::Type)),
				this, SIGNAL(ClearCacheRequested(CacheStatus::Type)));
	}
	setLayout(layout);
}
