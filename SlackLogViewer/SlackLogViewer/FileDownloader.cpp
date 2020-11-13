#include "filedownloader.h"

FileDownloader::FileDownloader(QUrl url)
{
	SetUrl(url);
}
FileDownloader::FileDownloader()
{}
FileDownloader::~FileDownloader()
{
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
	mManager->get(*mRequest);
}
QByteArray FileDownloader::GetDownloadedData() const
{
	return mDownloadedData;
}

void FileDownloader::ReplyFinished(QNetworkReply* reply)
{
	if (reply->error() == QNetworkReply::NoError)
	{
		mDownloadedData = reply->readAll();
		//emit a signal
		reply->deleteLater();
		emit Downloaded();
	}
	else
	{
		mError = reply->error();
		reply->deleteLater();
		emit DownloadFailed();
	}
}
