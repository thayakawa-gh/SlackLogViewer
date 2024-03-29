#ifndef DOCUMENTVIEW_H
#define DOCUMENTVIEW_H

#include <QFrame>
#include <QScrollArea>
#include <vector>

class QScrollArea;
class QTextEdit;
class QLabel;
class AttachedFile;
class ImageFile;
class TextFile;

class DocumentView : public QFrame
{
	Q_OBJECT
public:

	DocumentView();
	virtual ~DocumentView() = default;

Q_SIGNALS:

	void Closed();

public Q_SLOTS:

	void Open(const AttachedFile* i);

protected:

	QScrollArea* mScrollArea;
	const ImageFile* mImage;
	QLabel* mUserIcon;
	QLabel* mFileName;
	QLabel* mUserName;
	QLabel* mTimeStampStr;
};

class ImageScrollArea : public QScrollArea
{
	Q_OBJECT
protected:
	virtual void wheelEvent(QWheelEvent*) {}
};

class ImageView : public DocumentView
{
	Q_OBJECT
public:

	ImageView();

public Q_SLOTS:

	void OpenImage(const ImageFile* i);

	void FitToWindow();
	void ZoomIn(QWheelEvent* event);
	void ZoomOut(QWheelEvent* event);
	void Scale(QWheelEvent* event, int levelfrom, int levelto);

protected:

	virtual void wheelEvent(QWheelEvent* event);

private:

	ImageScrollArea* mScrollArea;
	const ImageFile* mImage;
	QLabel* mImageLabel;
	QLabel* mUserIcon;
	QLabel* mFileName;
	QLabel* mUserName;
	QLabel* mTimeStampStr;
	bool mFitted;
	int mScaleLevel;
	
	static constexpr int mDefaultLevel = 4;
	static const std::vector<double> mScales;
};

class TextView : public DocumentView
{
	Q_OBJECT
public:

	TextView();

public Q_SLOTS:

	void OpenText(const AttachedFile* t);

private:
	QTextEdit* mTextEdit;
};

class QWebEngineView;

class PDFView : public DocumentView
{
	Q_OBJECT
public:

	PDFView();
	virtual ~PDFView();

	void OpenPDF(const AttachedFile* t);

	QWebEngineView* mView;
};

#endif