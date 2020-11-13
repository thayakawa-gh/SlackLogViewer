#ifndef SEARCH_BAR_H
#define SEARCH_BAR_H

#include <QFrame>
#include <QLineEdit>
#include "SearchResultList.h"

class QLabel;
class QTreeWidget;
class QMenu;
class QComboBox;
class QPushButton;
class QLineEdit;

/*
class SearchBoxPopup : public QWidget
{
	Q_OBJECT
public:

	SearchBoxPopup(QLineEdit* parent = nullptr);
	virtual ~SearchBoxPopup();

	void ShowPopup();
	void HidePopup();

	SearchMode GetSearchMode() const;

	virtual bool eventFilter(QObject* obj, QEvent* ev) override;

signals:
	void SearchRequested();

private:

	QComboBox* mMatch;
	QPushButton* mCase;
	QPushButton* mRegex;
	QComboBox* mRange;
    QLineEdit* mParent;
};

class SearchBox : public QLineEdit
{
	Q_OBJECT
public:

	SearchBox(QWidget* parent = nullptr);

	virtual bool eventFilter(QObject* obj, QEvent* event) override;

signals:

	void SearchRequested(QString key, SearchMode s);

public slots:

	void ExecuteSearch();

private:
	SearchBoxPopup* mPopup;
};
*/


class SearchBox;
class InnerSearchBox : public QLineEdit
{
	Q_OBJECT
public:

	virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
};
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
	virtual void showEvent(QShowEvent* event) { emit Showed(); QWidget::showEvent(event); }
	virtual void hideEvent(QHideEvent* event) { emit Hidden(); QWidget::hideEvent(event); }
	virtual void closeEvent(QCloseEvent* event) override;

	QLineEdit* GetSearchBox() { return mBox; }

signals:

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

signals:

	void SearchRequested(QString key, SearchMode s);

public slots:

	void ExecuteSearch();

private:

	SearchBoxPopup* mPopup;
};

class MenuBar : public QFrame
{
	Q_OBJECT
public:

	enum Scope { FromCurrentChannel, FromAllChannels, };

	MenuBar(QMenu* menu, QWidget* parent = nullptr);

signals:

	void SearchRequested(QString key, SearchMode s);

private slots:


private:

	QLabel* mChannelName;
	QLineEdit* mSearchBox;

};


#endif