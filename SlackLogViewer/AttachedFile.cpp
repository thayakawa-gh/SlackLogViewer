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
{
	mType = type;
	auto size_it = o.find("size");
	if (size_it != o.end()) mFileSize = size_it.value().toInt();
	auto pretty_type_it = o.find("pretty_type");
	if (pretty_type_it != o.end()) mPrettyType = pretty_type_it.value().toString();
	auto url_it = o.find("url_private_download");
	if (url_it != o.end()) mUrl = url_it.value().toString();
	auto id_it = o.find("id");
	if (id_it != o.end()) mID = id_it.value().toString();
	auto name_it = o.find("name");
	if (name_it != o.end()) mFileName = name_it.value().toString();
	auto ext_it = o.find("filetype");
	if (ext_it != o.end()) mExtension = "." + ext_it.value().toString();
	auto user_it = o.find("user");
	if (user_it != o.end()) mUserID = user_it.value().toString();
	auto ts_it = o.find("timestamp");
	if (ts_it != o.end())
	{
		QDateTime dt;
		dt.setSecsSinceEpoch(ts_it.value().toInt());
		mTimeStampStr = dt.toString("yyyy/MM/dd hh:mm:ss");
	}
}
AttachedFile::AttachedFile(CacheType type, const QString& url, const QString& message_ts, int index)
	: mDownloader(nullptr)
{
	mType = type;
	mUrl = url;
	mID = message_ts + "_" + QString::number(index) + "_" + QUrl(url).fileName();
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
ImageFile::ImageFile(const QString & url, const QString& message_ts, int index)
	: AttachedFile(CacheType::IMAGE, url, message_ts, index)
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