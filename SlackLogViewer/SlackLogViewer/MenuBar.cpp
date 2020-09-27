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

SearchBoxPopup::SearchBoxPopup(QLineEdit* parent)
	: QWidget(nullptr), mParent(parent)
{
	this->setFixedWidth(parent->width());
	this->setStyleSheet("background-color: white; color: black; border: 1px solid grey; border-radius: 0px;");
	this->setWindowFlags(Qt::Popup | Qt::WindowStaysOnTopHint);
	this->installEventFilter(this);
	this->setAttribute(Qt::WA_ShowWithoutActivating);

	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(2, 2, 2, 2);
	//match
	mMatch = new QComboBox();
	{
		mMatch->addItem("Exact phrase");
		mMatch->addItem("All words");
		mMatch->addItem("Any words");
		mMatch->setFocusPolicy(Qt::NoFocus);
		mMatch->setStyleSheet("border-radius: 0px;");
		layout->addWidget(mMatch);
	}
	//case
	mCase = new QPushButton();
	{
		mCase->setCheckable(true);
		mCase->setIcon(QIcon("Resources\\upper_lower_case.png"));
		mCase->setToolTip("Case sensitive");
		mCase->setFocusPolicy(Qt::NoFocus);
		mCase->setStyleSheet("QPushButton:checked { color: white; background-color: lightblue; } QPushButton { border-radius: 0px; }");
		layout->addWidget(mCase);
	}
	//regex
	mRegex = new QPushButton();
	{
		mRegex->setCheckable(true);
		mRegex->setIcon(QIcon("Resources\\regex.png"));
		mRegex->setToolTip("Regular expression");
		mRegex->setFocusPolicy(Qt::NoFocus);
		mRegex->setStyleSheet("QPushButton:checked { color: white; background-color: lightblue; } QPushButton { border-radius: 0px; }");
		layout->addWidget(mRegex);

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
		layout->addWidget(mRange);
	}
	layout->addStretch();
	this->setLayout(layout);
}
SearchBoxPopup::~SearchBoxPopup()
{}
void SearchBoxPopup::ShowPopup()
{
	this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	this->move(mParent->mapToGlobal(QPoint(0, mParent->height())));
	this->show();
	this->setFocus();//一時的にFocusする。一度こちらをFocusしておかないと、クリック時のparentのFocusでカーソルが表示されない。
}
void SearchBoxPopup::HidePopup()
{
	this->hide();
}
SearchMode SearchBoxPopup::GetSearchMode() const
{
	int mindex = mMatch->currentIndex();
	SearchMode::Match m;
	switch (mMatch->currentIndex())
	{
	case 0: m = SearchMode::EXACTPHRASE; break;
	case 1: m = SearchMode::ALLWORDS; break;
	case 2: m = SearchMode::ANYWORDS; break;
	}
	SearchMode::Case c = mCase->isChecked() ? SearchMode::SENSITIVE : SearchMode::INSENSITIVE;
	SearchMode::Regex r = mRegex->isChecked() ? SearchMode::REGEX : SearchMode::KEYWORDS;
	SearchMode::Range n;
	switch (mRange->currentIndex())
	{
	case 0: n = SearchMode::CURRENTCH; break;
	case 1: n = SearchMode::ALLCHS; break;
	}
	return SearchMode(m, c, r, n);
}
bool SearchBoxPopup::eventFilter(QObject* obj, QEvent* ev)
{
	if (obj != this)
		return false;
	if (ev->type() == QEvent::MouseButtonDblClick ||
		ev->type() == QEvent::MouseButtonPress ||
		ev->type() == QEvent::MouseButtonRelease ||
		ev->type() == QEvent::MouseMove)
	{
		QMouseEvent* mouse = static_cast<QMouseEvent*>(ev);
		QPoint local = mParent->mapFromGlobal(mouse->globalPos());
		if (mParent->rect().contains(local))
		{
			mouse->setLocalPos(local);
			mParent->setFocus(Qt::MouseFocusReason);//これをMouseFocusReasonにしないとカーソルが表示されないらしい。
			mParent->event(ev);
			return true;
		}
		else if (!rect().contains(mouse->pos()))
		{
			hide();
			return true;
		}
	}
	else if (ev->type() == QEvent::KeyPress)
	{
		int key = static_cast<QKeyEvent*>(ev)->key();
		bool consumed = false;
		switch (key)
		{
		case Qt::Key_Enter:
		case Qt::Key_Return:
			emit SearchRequested();
			this->hide();
			consumed = true;
			break;
		default:
			mParent->event(ev);
			break;
		}
		return consumed;
	}
	return false;
}

SearchBox::SearchBox(QWidget* parent)
	: QLineEdit(parent)
{
	setStyleSheet("color: rgb(255, 255, 255); border: 1px solid rgb(160, 160, 160); border-radius: 5px;");
	//setClearButtonEnabled(true);Popupを弄った影響でclearbuttonが上手く動作しないので、無効化しておく。
	installEventFilter(this);
	setPlaceholderText("Search");
	setFixedSize(400, 24);
	mPopup = new SearchBoxPopup(this);
	connect(mPopup, &SearchBoxPopup::SearchRequested, this, &SearchBox::ExecuteSearch);
}

bool SearchBox::eventFilter(QObject* object, QEvent* event)
{
	if (object != this)
		return false;
	auto t = event->type();
	if (t == QEvent::MouseButtonPress)
	{
		event->ignore();
		QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
		if (mPopup->isHidden())
		{
			mPopup->ShowPopup();
		}
		setFocus(Qt::MouseFocusReason);
	}
	else if (t == QEvent::KeyPress)
	{
		if (mPopup->isHidden())
		{
			mPopup->ShowPopup();
			setFocus();
		}
	}
	return false;
}


void SearchBox::ExecuteSearch()
{
	const QString& str = this->text();
	if (str.isEmpty()) return;
	emit SearchRequested(str, mPopup->GetSearchMode());
}

MenuBar::MenuBar(QMenu* menu, QWidget* parent)
	: QFrame(parent)
{
	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(gSpacing, gSpacing, gSpacing, gSpacing);
	setStyleSheet("background-color: rgb(53,13,54); border: 0px solid rgb(160, 160, 160);");
	{
		QToolButton* menubutton = new QToolButton();
		menubutton->setIcon(QIcon("Resources\\menu.png"));
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
		exe->setIcon(QIcon("Resources\\search.png"));
		layout->addWidget(exe);
		connect(exe, SIGNAL(clicked()), mSearchBox, SLOT(ExecuteSearch()));
		layout->addStretch();
	}
	setLayout(layout);
}