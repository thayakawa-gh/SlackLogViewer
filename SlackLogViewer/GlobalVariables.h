#ifndef GLOBALTYPES_H
#define GLOBALTYPES_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QPixmap>
#include <QDateTime>
#include <memory>
#include <QSettings>
#include <QFont>
#include <QTextCodec>

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
	const QPixmap& GetIcon() const { return mIcon; }
	bool IsDeleted() const { return mDeleted; }
	bool IsAdmin() const { return mAdmin; }
	bool IsOwner() const { return mOwner; }
	void SetUserIcon(const QByteArray& data)
	{
		mIcon.loadFromData(data);
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
	QPixmap mIcon;

	bool mDeleted;
	bool mAdmin;
	bool mOwner;
};

class Channel
{
public:

	Channel();
	Channel(const QString& id, const QString& name);

	void SetChannelInfo(const QString& id, const QString& name);

	const QString& GetID() const { return mID; }
	const QString& GetName() const { return mName; }


private:

	QString mID;
	QString mName;
};

void Construct(QSettings& s);

extern std::unique_ptr<User> gEmptyUser;
extern std::unique_ptr<QImage> gTempImage;
extern std::unique_ptr<QImage> gDocIcon;
extern std::unique_ptr<QSettings> gSettings;
extern std::unique_ptr<double> gDPI;
extern QString gWorkspace;//workspaceと言いつつ、ログファイルのフォルダ名のこと。jsonファイル中にワークスペース名があればよかったのだが。
extern QMap<QString, User> gUsers;
extern QVector<Channel> gChannelVector;
extern QStringList gHiddenChannels;


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

inline double GetNamePointSize() { return GetBasePointSize() * 1.2; };
inline double GetReactionPointSize() { return GetBasePointSize() * 1.3; };
inline double GetDateTimePointSize() { return GetBasePointSize() * 0.9; }
inline double GetThreadPointSize() { return GetBasePointSize() * 1.; }

#endif