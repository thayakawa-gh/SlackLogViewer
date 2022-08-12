#ifndef SEARCH_BAR_H
#define SEARCH_BAR_H

#include <QFrame>
#include <QLineEdit>
#include "SearchResultList.h"
#include "CacheStatus.h"

class QLabel;
class QTreeWidget;
class QMenu;
class QComboBox;
class QPushButton;
class QLineEdit;

class SearchBox;
class QCalendarWidget;
class QDateEdit;

class InnerSearchBox : public QLineEdit
{
	Q_OBJECT
public:

	virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
};
/*class CalendarPopup : public QWidget
{
public:

	CalendarPopup();

private:

	QDateEdit* mBegin;
	QDateEdit* mEnd;
	QCalendarWidget* mCalendar;
};*/
class SearchBoxPopup : public QWidget
{
	Q_OBJECT
public:

	friend class SearchBox;

	SearchBoxPopup(SearchBox* parent = nullptr);
	//virtual ~SearchBoxPopup();

	void ShowPopup();
	void HidePopup();

	SearchMode GetSearchMode() const;

	//virtual bool eventFilter(QObject* obj, QEvent* ev) override;
	void PassEventToBox(QEvent* event_) { mBox->event(event_); }
	virtual void showEvent(QShowEvent* event) { Q_EMIT Showed(); QWidget::showEvent(event); }
	virtual void hideEvent(QHideEvent* event) { Q_EMIT Hidden(); QWidget::hideEvent(event); }
	virtual void closeEvent(QCloseEvent* event) override;

	QLineEdit* GetSearchBox() { return mBox; }

Q_SIGNALS:

	void Hidden();
	void Showed();
	void SearchRequested();

private:

	SearchBox* mParent;
	InnerSearchBox* mBox;

	QComboBox* mMatch;
	QPushButton* mCase;
	QPushButton* mRegex;
	QComboBox* mRange;

	QVector<QPushButton*> mTargets;
};

class SearchBox : public QLineEdit
{
	Q_OBJECT
public:

	SearchBox(QWidget* parent = nullptr);

	//virtual bool eventFilter(QObject* obj, QEvent* event) override;

	virtual void keyPressEvent(QKeyEvent* event) override;
	virtual void mousePressEvent(QMouseEvent* event) override;
	virtual void inputMethodEvent(QInputMethodEvent* event) override;
	virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

Q_SIGNALS:

	void SearchRequested(QString key, SearchMode s);

public Q_SLOTS:

	void ExecuteSearch();

private:

	SearchBoxPopup* mPopup;
};

class CacheStatus;

class MenuBar : public QFrame
{
	Q_OBJECT
public:

	MenuBar(QMenu* menu, QWidget* parent = nullptr);

Q_SIGNALS:

	void SearchRequested(QString key, SearchMode s);
	void CacheAllRequested(CacheStatus::Channel ch, CacheStatus::Type type);
	void ClearCacheRequested(CacheStatus::Type type);

private Q_SLOTS:


private:

	QLabel* mChannelName;
	SearchBox* mSearchBox;
};


#endif