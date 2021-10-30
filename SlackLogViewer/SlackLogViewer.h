#ifndef SLACKLOGVIEWER_H
#define SLACKLOGVIEWER_H

#include <QtWidgets/QMainWindow>
#include <QAbstractItemModel>
#include "GlobalVariables.h"
#include "Singleton.h"
#include "DocumentView.h"
#include "SearchResultList.h"
#include "CacheStatus.h"

class QStackedWidget;
class QListView;
class ReplyListView;
class Message;
class MessageHeaderWidget;
class UserProfileWidget;
class QSplitter;

class SlackLogViewer : public QMainWindow
{
	Q_OBJECT
public:

	SlackLogViewer(QWidget *parent = Q_NULLPTR);

private:

	void LoadSettings();
	void LoadUsers();
	void LoadChannels();

public slots:

	void UpdateRecentFiles();//History/LogFilePathsがアップデート済みであるのが前提。

	void OpenLogFileFolder();//フォルダのパスをダイアログから指定させる。
	void OpenLogFileZip();//zipファイルのパスをダイアログから指定させる。
	void OpenLogFile(const QString& path);//パスを引数で与える。
	void OpenOption();
	void Exit();

	void SetChannel(int ch);//チャンネルを切り替える。
	void SetChannelAndIndex(int ch, int row, int parentrow = -1);
	void JumpToPage(int page);

	void OpenThread(const Message* m);
	void OpenImage(const ImageFile* i);
	void OpenText(const AttachedFile* t);
	void OpenPDF(const AttachedFile* t);
	void OpenUserProfile(const User* u);
	void Search(const QString& key, SearchMode mode);

	//ch == -1のときは全チャンネル対象。
	void CacheAllFiles(CacheStatus::Channel ch, CacheStatus::Type type);
	void ClearCache(CacheStatus::Type type);

	void CloseDocument();

	void OpenCredit();

	void closeEvent(QCloseEvent* event);

private:

	QVector<QAction*> mOpenRecentActions;
	MessageHeaderWidget* mMessageHeader;
	QListView* mChannelView;
	QList<MessageListView*> mBrowsedChannels;//最大でNumOfChannelStorage個までのチャンネル情報を保管しておく。ここから溢れたものはメモリから削除する。
	QStackedWidget* mStack;
	QStackedWidget* mChannelPages;
	QSplitter* mSplitter;
	QStackedWidget* mRightStack;//thread、user profileの表示領域。
	QLabel* mRightStackLabel;//ThreadまたはProfileの文字を表示する。
	ReplyListView* mThread;
	UserProfileWidget* mProfile;
	ImageView* mImageView;
	TextView* mTextView;
	PDFView* mPDFView;
	SearchResultListView* mSearchView;
};


class ChannelListModel : public QAbstractListModel
{
public:
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const;
	//virtual int columnCount(const QModelIndex& parent = QModelIndex()) const;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const;
	//virtual QModelIndex parent(const QModelIndex& index) const;
	virtual bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex());
	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex());

	void SetChannelInfo(int row, const QString& id, const QString& name);
};


class QHBoxLayout;
class QPushButton;

class MessageHeaderWidget : public QFrame
{
	Q_OBJECT
public:

	static constexpr int msNumOfButtons = 5;

	MessageHeaderWidget();

signals:

	void JumpRequested(int page);

public slots:

	void Open(const QString& ch, int npages, int currentpage);
	void SetCurrentPage(int page);
	void JumpToTop();
	void JumpToPrev();
	void JumpToNext();
	void JumpToBottom();
	void JumpTo(int);

private:

	void Clear();

	int mCurrentPage;
	int mNumOfPages;

	QHBoxLayout* mPageNumberLayout;
	QLabel* mChannelLabel;
	QPushButton* mTop;
	QPushButton* mPrev;
	std::vector<QPushButton*> mNumbers;
	QPushButton* mNext;
	QPushButton* mBottom;
};

using MainWindow = Singleton<SlackLogViewer>;

#endif