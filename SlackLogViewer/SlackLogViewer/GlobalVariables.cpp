#include "GlobalVariables.h"
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <sstream>

#include <shlwapi.h>
#include <atlstr.h>
#include <locale.h>
#pragma comment(lib, "version.lib")
#undef RGB

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
}

extern VersionInfo gVersionInfo = VersionInfo();

QJsonDocument LoadJsonFile(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		exit(-1);
	}
	QByteArray data = file.readAll();
	return QJsonDocument::fromJson(data);
}

User::User(const QString& id, const QString& name, const QString& image)
	: mID(id), mName(name), mIconUrl(image)
{
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

void Construct(QSettings& s)
{
	if (!s.contains("Font/Size")) s.setValue("Font/Size", 9);
	if (!s.contains("Font/Family")) s.setValue("Font/Family", "メイリオ");
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

}

std::unique_ptr<User> gEmptyUser = nullptr;
std::unique_ptr<QImage> gTempImage = nullptr;
std::unique_ptr<QImage> gDocIcon = nullptr;
std::unique_ptr<QSettings> gSettings = nullptr;
std::unique_ptr<double> gDPI = nullptr;
QString gWorkspace = QString();
QMap<QString, User> gUsers = QMap<QString, User>();
QVector<Channel> gChannelVector = QVector<Channel>();
QStringList gHiddenChannels = QStringList();