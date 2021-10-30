#include <QLabel>
#include <QWheelEvent>
#include <QScroller>
#include <QScrollBar>
#include <QTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QtWebEngineWidgets>
#include "DocumentView.h"
#include "MessageListView.h"
#include "FileDownloader.h"

DocumentView::DocumentView()
{
	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	QHBoxLayout* bar = new QHBoxLayout();
	bar->setContentsMargins(gSpacing, gSpacing, gSpacing, gSpacing);
	{
		//ファイル名やボタン類
		mUserIcon = new QLabel();
		bar->addWidget(mUserIcon);
		QVBoxLayout* text = new QVBoxLayout();
		mFileName = new QLabel();
		QFont f1;
		f1.setBold(true);
		mFileName->setFont(f1);
		text->addWidget(mFileName);
		mTimeStampStr = new QLabel();
		QFont f2;
		mTimeStampStr->setFont(f2);
		text->addWidget(mTimeStampStr);
		bar->addLayout(text);
		bar->addStretch();

		QPushButton* close = new QPushButton();
		close->setCursor(Qt::PointingHandCursor);
		close->setStyleSheet("border: 0px;");
		QPixmap icon("Resources\\batsu.png");
		close->setIcon(icon);
		connect(close, &QPushButton::clicked, this, &DocumentView::Closed);
		bar->addWidget(close);
	}
	layout->addLayout(bar);
	setLayout(layout);
	setStyleSheet("background-color: rgb(255, 255, 255);");
}
void DocumentView::Open(const AttachedFile* f)
{
	mFileName->setText(f->GetFileName());
	auto it = gUsers.find(f->GetUserID());
	if (it != gUsers.end())
	{
		mUserIcon->setPixmap(it.value().GetIcon().scaled(QSize(gIconSize, gIconSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		mTimeStampStr->setText(it.value().GetName() + " " + f->GetTimeStampStr());
	}
	else
	{
		mUserIcon->setPixmap(QPixmap("Resources\\batsu.png").scaled(QSize(gIconSize, gIconSize), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		mTimeStampStr->setText("_______ " + f->GetTimeStampStr());
	}
}


ImageView::ImageView()
	: mFitted(false), mScaleLevel(mDefaultLevel), mImage(nullptr)
{
	QVBoxLayout* layout = static_cast<QVBoxLayout*>(this->layout());

	mScrollArea = new ImageScrollArea();
	mScrollArea->setStyleSheet("border: 0px;");
	mImageLabel = new QLabel();
	mScrollArea->setWidget(mImageLabel);
	mScrollArea->setAlignment(Qt::AlignCenter);
	QScroller::grabGesture(mScrollArea, QScroller::LeftMouseButtonGesture);
	mImageLabel->setBackgroundRole(QPalette::Base);
	mImageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	mImageLabel->setScaledContents(true);

	//falseのときは内部のウィジェットの大きさに従う。
	//trueの時はウィジェットの大きさを無視して自動リサイズしてしまう（QScrollAreaがウィジェットを強制リサイズできるようになる）。
	//ということらしい。
	mScrollArea->setWidgetResizable(false);
	mScrollArea->setVisible(true);

	layout->addWidget(mScrollArea);
}
void ImageView::OpenImage(const ImageFile* i)
{
	this->Open(i);
	mScaleLevel = mDefaultLevel;
	mFitted = false;
	mImage = i;
	mImageLabel->setPixmap(QPixmap::fromImage(i->GetImage()));
	mScrollArea->setWidgetResizable(false);
	mImageLabel->resize(mScales[mDefaultLevel] * mImageLabel->pixmap()->size());
}
void ImageView::FitToWindow()
{
	mScrollArea->setWidgetResizable(true);
}
void ImageView::ZoomIn(QWheelEvent* event)
{
	if (mScaleLevel < mScales.size() - 1)
	{
		mScaleLevel += 1;
		Scale(event, mScaleLevel - 1, mScaleLevel);
	}
}
void ImageView::ZoomOut(QWheelEvent* event)
{
	if (mScaleLevel > 0)
	{
		mScaleLevel -= 1;
		Scale(event, mScaleLevel + 1, mScaleLevel);
	}
}
void ImageView::Scale(QWheelEvent* event, int levelfrom, int levelto)
{
	double oldfactor = mScales[levelfrom];
	double factor = mScales[levelto];
	QPointF ScrollbarPos = QPointF(mScrollArea->horizontalScrollBar()->value(), mScrollArea->verticalScrollBar()->value());
	QPointF DeltaToPos = event->posF() / oldfactor - mScrollArea->widget()->pos() / oldfactor;
	QPointF Delta = DeltaToPos * factor - DeltaToPos * oldfactor;

	mImageLabel->resize(factor * mImageLabel->pixmap()->size());

	mScrollArea->horizontalScrollBar()->setValue(ScrollbarPos.x() + Delta.x());
	mScrollArea->verticalScrollBar()->setValue(ScrollbarPos.y() + Delta.y());
}
void ImageView::wheelEvent(QWheelEvent* event)
{
	if (event->angleDelta().y() > 0) ZoomIn(event);
	if (event->angleDelta().y() < 0) ZoomOut(event);
}
const std::vector<double> ImageView::mScales = std::vector<double>{ 0.125, 0.25, 0.50, 0.75, 1.00, 1.25, 1.50, 2.00, 2.50, 3.00, 4.00, 5.00 };

TextView::TextView()
{
	QVBoxLayout* layout = static_cast<QVBoxLayout*>(this->layout());
	mTextEdit = new QTextEdit();
	mTextEdit->setStyleSheet("border: 0px;");
	mTextEdit->setReadOnly(true);
	layout->addWidget(mTextEdit);
}

void TextView::OpenText(const AttachedFile* t)
{
	QFile file("Cache\\" + gWorkspace + "\\Text\\" + t->GetID());
	if (file.exists())
	{
		this->Open(t);
		file.open(QFile::ReadOnly | QFile::Text);
		mTextEdit->setPlainText(QString::fromUtf8(file.readAll()));
	}
	else
	{
		mTextEdit->setPlainText("Downloading.");
	}
}


PDFView::PDFView()
{
	QVBoxLayout* layout = static_cast<QVBoxLayout*>(this->layout());

	mView = new QWebEngineView(this);
	mView->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
	mView->settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);

	layout->addWidget(mView);
}
PDFView::~PDFView()
{
	mView->deleteLater();
}

void PDFView::OpenPDF(const AttachedFile* t)
{
	QString path = ".\\Cache\\" + gWorkspace + "\\PDF\\" + t->GetID();
	QFileInfo file(path);
	if (file.exists())
	{
		this->Open(t);
		//何故か相対パスだとファイルを開けない。絶対パスに変換する。
		QUrl url = QUrl::fromLocalFile(file.absoluteFilePath());
		mView->load(url);
	}
}