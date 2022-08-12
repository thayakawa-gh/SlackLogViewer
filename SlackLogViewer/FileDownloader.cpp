#include <QEventLoop>
#include "FileDownloader.h"

FileDownloader::FileDownloader(QUrl url)
{
	SetUrl(url);
}
FileDownloader::FileDownloader()
	: mError(QNetworkReply::NetworkError::NoError),
	mManager(nullptr), mRequest(nullptr), mReply(nullptr)
{}
FileDownloader::~FileDownloader()
{
	if (mReply) mReply->deleteLater();
	delete mManager;
	delete mRequest;
}
void FileDownloader::SetUrl(QUrl url)
{
	mUrl = url;
	mManager = new QNetworkAccessManager(this);
	mRequest = new QNetworkRequest(mUrl);
	connect(mManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(ReplyFinished(QNetworkReply*)));
	//リダイレクトを許す。でないとダウンロードできないファイルがある。
	mRequest->setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	mReply = mManager->get(*mRequest);
}
QByteArray FileDownloader::GetDownloadedData() const
{
	return mDownloadedData;
}
bool FileDownloader::Wait()
{
	QEventLoop loop;
	connect(mReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	if (!mReply->isFinished()) loop.exec();
	bool res = (mReply->error() == QNetworkReply::NoError);
	mError = mReply->error();
	return res;
}

void FileDownloader::ReplyFinished(QNetworkReply* reply)
{
	assert(reply == mReply);
	if (reply->error() == QNetworkReply::NoError)
	{
		mDownloadedData = reply->readAll();
		//emit a signal
		//reply->deleteLater();
		Q_EMIT Downloaded();
	}
	else
	{
		mError = reply->error();
		//reply->deleteLater();
		Q_EMIT DownloadFailed();
	}
}
