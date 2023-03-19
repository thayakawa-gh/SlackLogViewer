#include "AttachedFile.h"
#include "GlobalVariables.h"
#include <QJsonObject>
#include <QFile>

QString CachePath(const char* type, const QString& filename)
{
	return gCacheDir + gWorkspace + "/" + type + "/" + filename;
}
QString CachePath(CacheType type, const QString& id)
{
	switch (type)
	{
	case CacheType::TEXT: return CachePath("Text", id);
	case CacheType::IMAGE: return CachePath("Image", id);
	case CacheType::PDF: return CachePath("PDF", id);
	case CacheType::OTHERS: return CachePath("Others", id);
	}
	throw FatalError("unknown cache file type");
}

AttachedFile::AttachedFile(CacheType type, const QJsonObject& o)
	: mDownloader(nullptr)
{
	mType = type;
	mFileSize = o.find("size").value().toInt();
	mPrettyType = o.find("pretty_type").value().toString();
	mUrl = o.find("url_private_download").value().toString();
	mID = o.find("id").value().toString();
	mFileName = o.find("name").value().toString();
	mExtension = "." + o.find("filetype").value().toString();
	mUserID = o.find("user").value().toString();
	QDateTime dt;
	dt.setSecsSinceEpoch(o.find("timestamp").value().toInt());
	mTimeStampStr = dt.toString("yyyy/MM/dd hh:mm:ss");
}

QString AttachedFile::GetCacheFilePath() const
{
	return CachePath(mType, mID);
}
bool AttachedFile::CacheFileExists() const
{
	auto path = GetCacheFilePath();
	QFile f(path);
	return f.exists();
}
void AttachedFile::SaveCacheFile(const QByteArray& data) const
{
	auto path = CachePath(mType, mID);
	QFile f(path);
	f.write(data);
}

ImageFile::ImageFile(const QJsonObject& o)
	: AttachedFile(CacheType::IMAGE, o)
{}
/*void ImageFile::RequestDownload(FileDownloader* fd)
{
	if (HasImage())
	{
		Q_EMIT fd->Finished();
		fd->deleteLater();
		return;
	}
	QFile i(mID);
	if (i.exists())
	{
		SetImage(i.readAll());
		Q_EMIT fd->Finished();
		fd->deleteLater();
		return;
	}
	//スロットが呼ばれるタイミングは遅延しているので、
	//gUsersに格納されたあとのUserオブジェクトを渡しておく必要があるはず。
	fd->SetUrl(mUrl);
	QObject::connect(fd, &FileDownloader::Downloaded, [this, fd]()
					 {
						 QByteArray f = fd->GetDownloadedData();
						 QFile o(CachePath("Image", GetID()));
						 o.open(QIODevice::WriteOnly);
						 o.write(f);
						 SetImage(f);
						 Q_EMIT fd->Finished();
						 fd->deleteLater();
					 });
	QObject::connect(fd, &FileDownloader::DownloadFailed, [this, fd]
					 {
						 fd->deleteLater();
					 });
}*/
bool ImageFile::Load()
{
	if (HasImage()) return true;
	QFile f(GetCacheFilePath());
	if (!f.exists()) return false;
	if (!f.open(QIODevice::ReadOnly)) return false;
	mImage.loadFromData(f.readAll());
	return true;
}
void ImageFile::LoadFromByteArray(const QByteArray& data)
{
	if (HasImage()) return;
	mImage.loadFromData(data);
}

TextFile::TextFile(const QJsonObject& o)
	: AttachedFile(CacheType::TEXT, o)
{}
/*void TextFile::RequestDownload(FileDownloader* fd)
{
	if (IsLoaded())
	{
		Q_EMIT fd->Finished();
		fd->deleteLater();
		return;
	}
	//スロットが呼ばれるタイミングは遅延しているので、
	//gUsersに格納されたあとのUserオブジェクトを渡しておく必要があるはず。
	fd->SetUrl(mUrl);
	QObject::connect(fd, &FileDownloader::Downloaded, [this, fd]()
					 {
						 QByteArray f = fd->GetDownloadedData();
						 QFile o(CachePath("Image", GetID()));
						 o.open(QIODevice::WriteOnly);
						 o.write(f);
						 SetText(f);
						 Q_EMIT fd->Finished();
						 fd->deleteLater();
					 });
	QObject::connect(fd, &FileDownloader::DownloadFailed, [this, fd]
					 {
						 fd->deleteLater();
					 });
}*/

PDFFile::PDFFile(const QJsonObject& o)
	: AttachedFile(CacheType::PDF, o)
{}

OtherFile::OtherFile(const QJsonObject& o)
	: AttachedFile(CacheType::OTHERS, o)
{}