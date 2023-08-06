#ifndef GLOBALTYPES_H
#define GLOBALTYPES_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QPixmap>
#include <QIcon>
#include <QDateTime>
#include <memory>
#include <QSettings>
#include <QFont>
#include <QException>
//#include <QTextCodec>

class QDir;

enum Stage
{
	RELEASE = 0, RELEASECANDIDATE = -1, BETA = -2, ALPHA = -3, PREALPHA = -4,
};
struct VersionNumber
{
	int MAJOR_VER;
	int MINOR_VER;
	Stage STAGE_VER;
	int REVISION_VER;
};
struct VersionInfo
{
public:

	VersionInfo();

	const std::string& GetVersionNumberStr() const { return mVersionNumberStr; }
	const std::string& GetFileDescription() const { return mFileDescription; }
	const std::string& GetCompanyName() const { return mCompanyName; }
	const std::string& GetCopyright() const { return mCopyright; }

private:
	VersionNumber mVersionNumber;
	std::string mVersionNumberStr;
	std::string mFileDescription;
	std::string mCompanyName;
	std::string mCopyright;
};

struct FatalError : public QException
{
	//FatalError(std::exception& err) : mError(err) {}
	FatalError(const char* str) : mError(str) {}
	FatalError(const QByteArray& arr) : mError(arr) {}
	FatalError(const QString& str) : mError(str) {}
	virtual void raise() const { throw* this; }
	virtual QException* clone() const { return new FatalError(*this); }
	//std::exception error() const { return mError; }
	QString error() const { return mError; }
private:
	QString mError;
};

extern VersionInfo gVersionInfo;

QJsonDocument LoadJsonFile(const QString& folder_or_zip, const QString& path);
QVector<std::pair<QDateTime, QString>> GetMessageFileList(const QString& folder_or_zip, const QString& channel);

class User
{
public:

	//User(const QString& id, const QString& name, const QString& realname, const QString& image);
	User(const QJsonObject& o);
	User();//空のユーザーを作成する。削除済みユーザーなどはこれの扱い。

	const QString& GetID() const { return mID; }
	const QString& GetName() const { return mName; }
	const QString& GetRealName() const { return mRealName; }
	const QString& GetTimeZone() const { return mTimeZone; }
	const QString& GetTitle() const { return mTitle; }
	const QString& GetPhone() const { return mPhone; }
	const QString& GetEmail() const { return mEmail; }
	const QString& GetSkype() const { return mSkype; }
	const QString& GetIconUrl() const { return mIconUrl; }
	const QPixmap& GetPixmap() const { return mPixmap; }
	const QIcon& GetIcon() const { return mIcon; }
	bool IsDeleted() const { return mDeleted; }
	bool IsAdmin() const { return mAdmin; }
	bool IsOwner() const { return mOwner; }
	void SetUserIcon(const QByteArray& data)
	{
		mPixmap.loadFromData(data);
		mIcon = mPixmap;
	}

private:

	QString mID;
	QString mName;
	QString mRealName;
	QString mTimeZone;
	QString mTimeZoneLabel;
	QString mTitle;
	QString mPhone;
	QString mEmail;
	QString mSkype;
	QString mIconUrl;
	QPixmap mPixmap;
	QIcon mIcon;

	bool mDeleted;
	bool mAdmin;
	bool mOwner;
};

class Channel
{
public:

	enum Type { CHANNEL, DIRECT_MESSAGE, GROUP_MESSAGE, END_CH, };

	Channel();
	Channel(Type type, const QString& id, const QString& name, const QVector<QString>& members);
	Channel(Type type, bool is_private, const QString& id, const QString& name, const QVector<QString>& members);

	void SetChannelInfo(Type type, const QString& id, const QString& name, const QVector<QString>& members);
	void SetChannelInfo(Type type, bool is_private, const QString& id, const QString& name, const QVector<QString>& members);

	Type GetType() const { return mType; }
	bool IsPrivate() const { return mIsPrivate; }
	const QString& GetID() const { return mID; }
	const QString& GetName() const { return mName; }
	//Channelの場合はディレクトリはチャンネル名だが、DM、GMではなぜかIDなので、その差を吸収する。
	const QString& GetDirName() const
	{
		if (mType == DIRECT_MESSAGE) return mID;
		return mName;
	}
	const QVector<QString>& GetMembers() const { return mMembers; }

	bool IsParent() const { return mID.isEmpty() && mName.isEmpty(); }

private:

	Type mType;
	bool mIsPrivate;
	QString mID;
	QString mName;
	QVector<QString> mMembers;
};

const Channel& GetChannel(Channel::Type type, int index);
const Channel& GetChannel(int row);

int IndexToRow(Channel::Type type, int index);
std::pair<Channel::Type, int> RowToIndex(int row);

void Construct(QSettings& s, const QDir& exedir);

QString ResourcePath(const char* filename);

enum class CacheType { TEXT, IMAGE, PDF, OTHERS, ALL, };
QString CacheTypeToString(CacheType type);
/*
QString CachePath(const char* type, const char* filename);
QString CachePath(const char* type, const QString& filename);
QString CachePath(const QString& type, const char* filename);
QString CachePath(const QString& type, const QString& filename);
QString CachePath(const char* type);
QString CachePath(const QString& type);
*/
QString CachePath(CacheType type);

QString IconPath(const QString& id);
QString IconPath();

inline bool Contains(const QString& src, const QString& phrase, bool case_)
{
	return src.contains(phrase, case_ ? Qt::CaseSensitive : Qt::CaseInsensitive);
}
inline bool Contains(const QString& src, const QStringList& keys, bool case_, bool all)
{
	bool found = all;
	for (const auto& k : keys)
	{
		bool contains = src.contains(k, case_ ? Qt::CaseSensitive : Qt::CaseInsensitive);
		//QTextCursor c = d->find(k, 0, f);
		if (all)
		{
			//一つでも含まれなかった場合は見つからなかった扱い。
			if (!contains)
			{
				found = false;
				break;
			}
		}
		else
		{
			//一つでも含まれればOK。
			if (contains)
			{
				found = true;
				break;
			}
		}
	}
	return found;
}
inline bool Contains(const QString& src, const QRegularExpression& regex)
{
	return src.contains(regex);
}

extern std::unique_ptr<User> gEmptyUser;
extern std::unique_ptr<QImage> gTempImage;
extern std::unique_ptr<QImage> gDocIcon;
extern std::unique_ptr<QSettings> gSettings;
extern std::unique_ptr<double> gDPI;
extern std::unique_ptr<bool> gDateSeparator;
extern QString gWorkspace;//workspaceと言いつつ、ログファイルのフォルダ名のこと。jsonファイル中にワークスペース名があればよかったのだが。
extern QString gResourceDir;
extern QString gCacheDir;
extern QMap<QString, User> gUsers;
extern QVector<Channel> gChannelParentVector;
extern QVector<Channel> gChannelVector;
extern QVector<Channel> gPrivateChannelVector;
extern QVector<Channel> gDMUserVector;
extern QVector<Channel> gGMUserVector;


inline static const int gHeaderHeight = 48;

inline static const int gTopMargin = 8;
inline static const int gBottomMargin = 8;
inline static const int gRightMargin = 16;
inline static const int gLeftMargin = 16;

inline static const int gSpacing = 8;

inline static const int gIconSize = 36;
inline static const int gThreadButtonHeight = 32;//gThreadIconSize + 偶数にすること。
inline static const int gThreadIconSize = 24;

inline static const int gMaxThumbnailHeight = 240;
inline static const int gMaxThumbnailWidth = 360;

inline double GetBasePointSize() { return gSettings->value("Font/Size").toInt(); }

inline double GetMessagePointSize() { return GetBasePointSize() * 1.2; }
inline double GetNamePointSize() { return GetBasePointSize() * 1.2; };
inline double GetReactionPointSize() { return GetBasePointSize() * 1.3; };
inline double GetDateTimePointSize() { return GetBasePointSize() * 0.9; }
inline double GetThreadPointSize() { return GetBasePointSize() * 1.; }

#endif