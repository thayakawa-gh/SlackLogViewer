#ifndef ATTACHED_FILE_H
#define ATTACHED_FILE_H

#include <type_traits>
#include <optional>
#include <QString>
#include <QFile>
#include <QObject>
#include "FileDownloader.h"
#include "GlobalVariables.h"

class QJsonObject;

class AttachedFile
{
public:

	//enum Type { TEXT, IMAGE, PDF, OTHERS, };

	AttachedFile(CacheType type, const QJsonObject& o);
	AttachedFile(CacheType type, const QString& url, const QString& message_ts, int index);//quote用。

	CacheType GetType() const { return mType; }
	bool IsText() const { return mType == CacheType::TEXT; }
	bool IsImage() const { return mType == CacheType::IMAGE; }
	bool IsPDF() const { return mType == CacheType::PDF; }
	bool IsOther() const { return mType == CacheType::OTHERS; }

	//Load系関数は現状、画像の場合にのみ意味を持つ。

	//ファイルがメモリにロードされているかどうかを返す。画像以外のファイルでは常にfalse。
	virtual bool IsLoaded() const { return false; };
	//キャッシュファイルをメモリにロードし、成功した場合はtrueを返す。画像以外のファイルでは常にfalse。
	virtual bool Load() { return false; }
protected:
	//データからメモリにロードする。画像以外の場合はなにもしない。
	virtual void LoadFromByteArray(const QByteArray&) {}
public:

	int GetFileSize() const { return mFileSize; }
	const QString& GetID() const { return mID; }
	const QString& GetPrettyType() const { return mPrettyType; }
	const QString& GetUrl() const { return mUrl; }
	const QString& GetFileName() const { return mFileName; }
	const QString& GetUserID() const { return mUserID; }
	const QString& GetTimeStampStr() const { return mTimeStampStr; }

	QString GetCacheFilePath() const;
	bool CacheFileExists() const;
protected:
	void SaveCacheFile(const QByteArray& data) const;
public:

	template <class FAlready, class FWait, class FSucceeded, class FFailed>
	void Download(FAlready already, FWait wait, FSucceeded succeeded, FFailed failed) const
	{
		//既に読み込まれている場合、もしくは既にファイルがある場合、alreadyを即時呼び出す。
		if (IsLoaded())
		{
			if constexpr (!std::is_same_v<FAlready, std::nullptr_t>) already(this);
			return;
		}
		if (CacheFileExists())
		{
			if constexpr (!std::is_same_v<FAlready, std::nullptr_t>) already(this);
			return;
		}
		//ファイルがない場合、
		//ダウンロード要求がない場合、まずはそちらを作成し、succeededとfailedをconnectする。
		//その後、waitを呼ぶ。
		if (mDownloader == nullptr)
		{
			mDownloader = new FileDownloader();
			mDownloader->RequestDownload(mUrl);

			QObject::connect(mDownloader, &FileDownloader::Downloaded, [this, succeeded]()
							 {
								 //ダウンロードできた場合、まずはファイルを保存し、その後succeededを呼ぶ。
								 QByteArray f = mDownloader->GetDownloadedData();
								 QFile o(GetCacheFilePath());
								 o.open(QIODevice::WriteOnly);
								 o.write(f);
								 mDownloader->deleteLater();
								 mDownloader = nullptr;
								 if constexpr (!std::is_same_v<FSucceeded, std::nullptr_t>)
									 succeeded(this);
							 });

			QObject::connect(mDownloader, &FileDownloader::DownloadFailed, [this, failed]()
							 {
								 mDownloader->deleteLater();
								 mDownloader = nullptr;
								 if constexpr (!std::is_same_v<FFailed, std::nullptr_t>)
									 failed(this);
							 });
		}
		else
		{
			if constexpr (!std::is_same_v<FSucceeded, std::nullptr_t>)
				QObject::connect(mDownloader, &FileDownloader::Downloaded, [this, succeeded]()
								 {
									 succeeded(this);
								 });
			if constexpr (!std::is_same_v<FFailed, std::nullptr_t>)
				QObject::connect(mDownloader, &FileDownloader::DownloadFailed, [this, failed]()
								 {
									 failed(this);
								 });
		}
		if constexpr (!std::is_same_v<FWait, std::nullptr_t>) wait(this);
	}

	//ファイルをダウンロードし開くことを要求する。
	//引数に与えられた関数をconnectする。
	//引数1. 既にファイルがある場合に即時呼ばれる。
	//引数2. ダウンロードの待機が発生したときに呼ばれる。
	//引数3. ダウンロードに完了したときに呼ばれる。引数が(const AttachedFile*, const QByteArray&)の2個あることに注意。
	//引数4. ダウンロードに失敗すると呼ばれる。
	//もしダウンロード後の処理が不要であれば、関数の代わりにnullptrを与えればよい。
	template <class FAlready, class FWait, class FSucceeded, class FFailed>
	void DownloadAndOpen(FAlready already, FWait wait, FSucceeded succeeded, FFailed failed)
	{
		//既に読み込まれている場合、もしくは既にファイルがある場合、alreadyを即時呼び出す。
		if (IsLoaded())
		{
			if constexpr (!std::is_same_v<FAlready, std::nullptr_t>) already(this);
			return;
		}
		if (CacheFileExists())
		{
			Load();
			if constexpr (!std::is_same_v<FAlready, std::nullptr_t>) already(this);
			return;
		}
		//ファイルがない場合、
		//ダウンロード要求がない場合、まずはそちらを作成し、succeededとfailedをconnectする。
		//その後、waitを呼ぶ。
		if (mDownloader == nullptr)
		{
			mDownloader = new FileDownloader();
			mDownloader->RequestDownload(mUrl);

			QObject::connect(mDownloader, &FileDownloader::Downloaded, [this, succeeded]()
							 {
								 //ダウンロードできた場合、まずはファイルを保存し、その後succeededを呼ぶ。
								 QByteArray f = mDownloader->GetDownloadedData();
								 LoadFromByteArray(f);
								 QFile o(GetCacheFilePath());
								 o.open(QIODevice::WriteOnly);
								 o.write(f);
								 mDownloader->deleteLater();
								 mDownloader = nullptr;
								 if constexpr (!std::is_same_v<FSucceeded, std::nullptr_t>)
									 succeeded(this);
							 });

			QObject::connect(mDownloader, &FileDownloader::DownloadFailed, [this, failed]()
							 {
								 mDownloader->deleteLater();
								 mDownloader = nullptr;
								 if constexpr (!std::is_same_v<FFailed, std::nullptr_t>)
									 failed(this);
							 });
		}
		else
		{
			if constexpr (!std::is_same_v<FSucceeded, std::nullptr_t>)
				QObject::connect(mDownloader, &FileDownloader::Downloaded, [this, succeeded]()
								 {
									 //mDownloaderを作成していない場合、保存の必要はない。
									 succeeded(this);
								 });
			if constexpr (!std::is_same_v<FFailed, std::nullptr_t>)
				QObject::connect(mDownloader, &FileDownloader::DownloadFailed, [this, failed]()
								 {
									 failed(this);
								 });
		}
		if constexpr (!std::is_same_v<FWait, std::nullptr_t>) wait(this);
	}

	bool Wait() const
	{
		if (mDownloader == nullptr) return true;
		return mDownloader->Wait();
	}

	bool FileNameContains(const QString& phrase, bool case_) const
	{
		return ::Contains(GetFileName(), phrase, case_);
	}
	bool FileNameContains(const QStringList& keys, bool case_, bool all) const
	{
		return ::Contains(GetFileName(), keys, case_, all);
	}
	bool FileNameContains(const QRegularExpression& regex) const
	{
		return ::Contains(GetFileName(), regex);
	}
	virtual bool FileContentsContains(const QString& /*phrase*/, bool /*case_*/) const { return false; }
	virtual bool FileContentsContains(const QStringList& /*keys*/, bool /*case_*/, bool /*all*/) const { return false; }
	virtual bool FileContentsContains(const QRegularExpression& /*regex*/) const { return false; }

protected:

	CacheType mType = CacheType::OTHERS;
	int mFileSize = 0;
	mutable FileDownloader* mDownloader = nullptr;
	QString mID;
	QString mPrettyType;
	QString mUrl;
	QString mFileName;
	QString mExtension;
	QString mUserID;
	QString mTimeStampStr;
};
class ImageFile : public AttachedFile
{
public:

	ImageFile(const QJsonObject& o);
	ImageFile(const QString& url, const QString& message_ts, int index);
	const QImage& GetImage() const { return mImage; }
	void ClearImage() { mImage = QImage(); }
	bool HasImage() const { return !mImage.isNull(); }
	virtual bool IsLoaded() const { return HasImage(); }
	//void RequestDownload(FileDownloader* fd);
	//bool LoadImage();
	virtual bool Load();
	virtual void LoadFromByteArray(const QByteArray& data);

private:

	//void SetImage(const QByteArray& data)
	//{
	//	mImage.loadFromData(data);
	//}

	QImage mImage;
};
class TextFile : public AttachedFile
{
public:

	TextFile(const QJsonObject& o);
	//void SetText(const QByteArray& data)
	//{
	//	mText = data;
	//}
	//void ClearText() { mText.reset(); }
	//virtual bool IsLoaded() const { return mText.has_value(); }
	//void RequestDownload(FileDownloader* fd);

private:
	//std::optional<QByteArray> mText;
};
class PDFFile : public AttachedFile
{
public:

	PDFFile(const QJsonObject& o);

private:
};
class OtherFile : public AttachedFile
{
public:
	OtherFile(const QJsonObject& o);
};

QByteArray LoadCacheFile(CacheType type, const QString& id);
bool CacheFileExists(CacheType type, const QString& id);
void SaveCacheFile(CacheType type, const QString& id, const QString& name);

#endif