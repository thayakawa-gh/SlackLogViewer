#include "GlobalVariables.h"
#include <QSettings>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <sstream>
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#ifdef _WIN32
#include <shlwapi.h>
#include <atlstr.h>
#include <locale.h>
#pragma comment(lib, "version.lib")
#undef RGB
#else
#include <Version.h>
#endif

std::string MakeVersionStr(VersionNumber n)
{
	std::string v = std::to_string(n.MAJOR_VER) + "." + std::to_string(n.MINOR_VER) + ".";
	switch (n.STAGE_VER)
	{
	case Stage::RELEASE: break;
	case Stage::RELEASECANDIDATE: v += "RC-"; break;
	case Stage::BETA: v += "Beta-"; break;
	case Stage::ALPHA: v += "Alpha-"; break;
	case Stage::PREALPHA: v += "PreAlpha-"; break;
	}
	v += std::to_string(n.REVISION_VER);
	return v;
}

inline std::vector<std::string> SplitStr(const std::string& str, char delim)
{
	std::vector<std::string> res;
	std::stringstream ss(str);
	std::string buffer;
	while (std::getline(ss, buffer, delim))
	{
		res.push_back(buffer);
	}
	return std::move(res);
}
VersionInfo::VersionInfo()
{
#ifdef _WIN32
	const int MAX_LEN = 2048;
	CString csMsg;
	CString csBuf;
	UINT uDmy;
	struct LANGANDCODEPAGE {
		WORD wLanguage; //日本語(0411)、英語(0409)
		WORD wCodePage;
	} *lpTran;

	LPTSTR    pBuf = csBuf.GetBuffer(MAX_LEN + 1);
	::GetModuleFileName(NULL, pBuf, MAX_LEN + 1);
	DWORD dwZero = 0;
	DWORD dwVerInfoSize = GetFileVersionInfoSize(pBuf, &dwZero);
	unsigned char* pBlock = NULL;
	pBlock = new unsigned char[dwVerInfoSize];
	::GetFileVersionInfo(pBuf, 0, dwVerInfoSize, pBlock);
	::VerQueryValue(pBlock, L"\\VarFileInfo\\Translation", (LPVOID*)&lpTran, &uDmy);

	//バージョンを取得するためのバッファ
	wchar_t name[256];
	void* pVer;

	std::size_t len = 0;
	char buf[1024];

	wsprintf(name, L"\\StringFileInfo\\%04x%04x\\ProductVersion", lpTran[0].wLanguage, lpTran[0].wCodePage);
	::VerQueryValue(pBlock, name, &pVer, &uDmy);
	wcstombs_s(&len, buf, 1024, (const wchar_t*)pVer, _TRUNCATE);
	std::vector<std::string> sp = SplitStr(buf, '.');
	mVersionNumber = VersionNumber{ std::stoi(sp[0]), std::stoi(sp[1]), Stage(-std::stoi(sp[2])), std::stoi(sp[3]) };
	mVersionNumberStr = MakeVersionStr(mVersionNumber);

	wsprintf(name, L"\\StringFileInfo\\%04x%04x\\FileDescription", lpTran[0].wLanguage, lpTran[0].wCodePage);
	::VerQueryValue(pBlock, name, &pVer, &uDmy);
	len = 0;
	wcstombs_s(&len, buf, 1024, (const wchar_t*)pVer, _TRUNCATE);
	mFileDescription = buf;

	wsprintf(name, L"\\StringFileInfo\\%04x%04x\\CompanyName", lpTran[0].wLanguage, lpTran[0].wCodePage);
	::VerQueryValue(pBlock, name, &pVer, &uDmy);
	len = 0;
	wcstombs_s(&len, buf, 1024, (const wchar_t*)pVer, _TRUNCATE);
	mCompanyName = buf;

	wsprintf(name, L"\\StringFileInfo\\%04x%04x\\LegalCopyright", lpTran[0].wLanguage, lpTran[0].wCodePage);
	::VerQueryValue(pBlock, name, &pVer, &uDmy);
	len = 0;
	wcstombs_s(&len, buf, 1024, (const wchar_t*)pVer, _TRUNCATE);
	mCopyright = buf;

	delete[] pBlock;
#else
        mVersionNumber = VersionNumber{ std::stoi(PROJECT_VER_MAJOR), std::stoi(PROJECT_VER_MINOR), Stage(-std::stoi(PROJECT_VER_PATCH)), std::stoi(PROJECT_VER_REVISION) };
        mVersionNumberStr = MakeVersionStr(mVersionNumber);
        mFileDescription = "SlackLogViewer";
        mCopyright = COMPANY_COPYRIGHT;
#endif
}

extern VersionInfo gVersionInfo = VersionInfo();

QJsonDocument LoadJsonFile(const QString& folder_or_zip, const QString& path)
{
	QFileInfo info(folder_or_zip);
	if (info.isDir())
	{
		QFile file(folder_or_zip + "\\" + path);
		if (!file.open(QIODevice::ReadOnly)) exit(-1);
		QByteArray data = file.readAll();
		return QJsonDocument::fromJson(data);
	}
	else
	{
		if (info.suffix() != "zip") exit(-1);
		QuaZipFile file(folder_or_zip, path);
		if (!file.open(QIODevice::ReadOnly)) exit(-1);
		QByteArray data = file.readAll();
		return QJsonDocument::fromJson(data);
	}
}
QVector<std::pair<QDateTime, QString>> GetMessageFileList(const QString& folder_or_zip, const QString& channel)
{
	QFileInfo info(folder_or_zip);
	QVector<std::pair<QDateTime, QString>> res;
	if (info.isDir())
	{
		QDir dir = gSettings->value("History/LastLogFilePath").toString() + "\\" + channel;
		auto ch_it = std::find_if(gChannelVector.begin(), gChannelVector.end(), [&channel](const Channel& ch) { return ch.GetName() == channel; });
		int ch_index = ch_it - gChannelVector.begin();
		QStringList ext = { "*.json" };//jsonファイルだけ読む。そもそもjson以外存在しないけど。
		QStringList filenames = dir.entryList(ext, QDir::Files, QDir::Name);
		res.reserve(filenames.size());
		for (auto& name : filenames)
		{
			int begin = name.indexOf('/');
			int end = name.lastIndexOf('.', begin);
			QDateTime d = QDateTime::fromString(name.mid(begin, end));
			res.push_back(std::pair(d, name));
		}
	}
	else
	{
		if (info.suffix() != "zip") exit(-1);
		QuaZip zip(folder_or_zip);
		zip.open(QuaZip::mdUnzip);
		zip.setFileNameCodec("UTF-8");
		QuaZipFileInfo64 info;
		for (bool b = zip.goToFirstFile(); b; b = zip.goToNextFile())
		{
			zip.getCurrentFileInfo(&info);
			if (!info.name.startsWith(channel)) continue;
			if (info.name.endsWith("/")) continue;
			int begin = info.name.indexOf('/');
			int end = info.name.lastIndexOf('.');
			auto dstr = info.name.mid(begin + 1, end - begin - 1);
			QDateTime d = QDateTime::fromString(dstr, Qt::ISODate);
			res.push_back(std::pair(d, info.name));
		}
	}
	//ファイルを日付順にソートする。
	std::sort(res.begin(), res.end(),
			  [](const std::pair<QDateTime, QString>& name1, const std::pair<QDateTime, QString>& name2)
			  {
				  return name1.first < name2.first;
			  });

	return res;
}

/*User::User(const QString& id, const QString& name, const QString& image)
	: mID(id), mName(name), mIconUrl(image)
{
}*/
QString GetString(const QJsonObject& o, const char* key)
{
	auto it = o.find(key);
	if (it == o.end()) return QString();
	return it.value().toString();
}
bool GetBool(const QJsonObject& o, const char* key)
{
	auto it = o.find(key);
	if (it == o.end()) return false;
	return it.value().toBool();
}
User::User(const QJsonObject& o)
{
	mID = GetString(o, "id");
	mName = GetString(o, "name");
	mRealName = GetString(o, "real_name");
	mTimeZone = GetString(o, "tz");
	mTimeZoneLabel = GetString(o, "tz_label");
	auto prof = o.find("profile").value().toObject();
	mTitle = GetString(prof, "title");
	mPhone = GetString(prof, "phone");
	mEmail = GetString(prof, "email");
	mSkype = GetString(prof, "skype");
	mIconUrl = GetString(prof, "image_192");

	mDeleted = GetBool(o, "deleted");
	mOwner = GetBool(o, "is_owner");
	mAdmin = GetBool(o, "is_admin");
}
User::User()
{}

Channel::Channel()
{}
Channel::Channel(const QString& id, const QString& name)
{
	SetChannelInfo(id, name);
}
void Channel::SetChannelInfo(const QString& id, const QString& name)
{
	mID = id;
	mName = name;
}

QString GetCacheDirFromEnv()
{
#ifdef _WIN32
	return qgetenv("LocalAppData");
#elif defined __APPLE__
	QString dir = qgetenv("HOME");
	if (dir.isEmpty()) return "";
	return dir + "/Library/Caches";
#elif defined __linux__
	QString dir = qgetenv("XDG_CACHE_HOME");
	if (!dir.isEmpty()) return dir;
	dir = getenv("HOME");
	if (!dir.isEmpty()) return dir + "/.cache";
	return "";
#endif
}
QString GetCacheDirBundled(const QDir& exedir)
{
	QDir tmp = exedir;
	QString tmpstr = tmp.absolutePath();
#ifdef __APPLE__
	tmp.cdUp();
	tmp.cd("Resources");
	if (!tmp.exists("Cache")) tmp.mkdir("Cache");
	tmp.cd("Cache");
	return tmp.absolutePath();
#else
	if (!tmp.exists("Cache")) tmp.mkdir("Cache");
	tmp.cd("Cache");
	return tmp.absolutePath();
#endif
}
QString GetDefaultCacheDir(const QDir& exedir)
{
	QString env = GetCacheDirFromEnv();
	if (!env.isEmpty())
	{
		//環境変数が見つかった場合は、その場所にSlackLogViewer_cacheディレクトリを作り、そのパスを返す。
		QDir envcache = env;
		if (!envcache.exists("SlackLogViewer_cache")) envcache.mkdir("SlackLogViewer_cache");
		return env + "/SlackLogViewer_cache";
	}
	return GetCacheDirBundled(exedir);
}

void Construct(QSettings& s, const QDir& exedir)
{
	if (!s.contains("Font/Size")) s.setValue("Font/Size", 9);
	if (!s.contains("Font/Family")) s.setValue("Font/Family", "Meiryo");
	if (!s.contains("ScrollStep")) s.setValue("ScrollStep", 24);
	if (!s.contains("NumOfMessagesPerPage")) s.setValue("NumOfMessagesPerPage", 100);
	if (!s.contains("History/LogFilePaths")) s.setValue("History/LogFilePaths", QStringList());
	if (!s.contains("History/NumOfRecentLogFilePaths")) s.setValue("History/NumOfRecentLogFilePaths", 8);
	if (!s.contains("History/LastLogFilePath")) s.setValue("History/LastLogFilePath", QStringList());
	if (!s.contains("History/NumOfChannelStorage")) s.setValue("History/NumOfChannelStorage", 5);
	if (!s.contains("Search/Match")) s.setValue("Search/Match", 0);
	if (!s.contains("Search/Case")) s.setValue("Search/Case", false);
	if (!s.contains("Search/Regex")) s.setValue("Search/Regex", false);
	if (!s.contains("Search/Range")) s.setValue("Search/Range", 0);
	if (!s.contains("Search/Filter")) s.setValue("Search/Filter", QVariantList{ true, false, false, false });
	if (!s.contains("Cache/Location")) s.setValue("Cache/Location", GetDefaultCacheDir(exedir));
}

QString ResourcePath(const char* filename)
{
	return gResourceDir + filename;
}
QString CachePath(const char* type, const char* filename)
{
	return gCacheDir + gWorkspace + "/" + type + "/" + filename;
}
QString CachePath(const char* type, const QString& filename)
{
	return gCacheDir + gWorkspace + "/" + type + "/" + filename;
}
QString CachePath(const QString& type, const char* filename)
{
	return gCacheDir + gWorkspace + "/" + type + "/" + filename;
}
QString CachePath(const QString& type, const QString& filename)
{
	return gCacheDir + gWorkspace + "/" + type + "/" + filename;
}
QString CachePath(const char* type)
{
	return gCacheDir + gWorkspace + "/" + type;
}
QString CachePath(const QString& type)
{
	return gCacheDir + gWorkspace + "/" + type;
}


std::unique_ptr<User> gEmptyUser = nullptr;
std::unique_ptr<QImage> gTempImage = nullptr;
std::unique_ptr<QImage> gDocIcon = nullptr;
std::unique_ptr<QSettings> gSettings = nullptr;
std::unique_ptr<double> gDPI = nullptr;
QString gWorkspace = QString();
QString gResourceDir = QString();
QString gCacheDir = QString();
QMap<QString, User> gUsers = QMap<QString, User>();
QVector<Channel> gChannelVector = QVector<Channel>();
QStringList gHiddenChannels = QStringList();
