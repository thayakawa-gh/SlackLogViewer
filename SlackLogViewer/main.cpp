#include "SlackLogViewer.h"
#include "GlobalVariables.h"
#include <QSettings>
#include <QtWidgets/QApplication>
#include <QTextCodec>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>

int main(int argc, char *argv[])
{
	QDir exedir = QCoreApplication::applicationDirPath();
#ifdef __APPLE__
	exedir.cdUp();
	gResourceDir = exedir.absolutePath() + "/Resources/";
	QDir tmp = QDir::temp();
	if (!tmp.exists("SlackLogViewer")) tmp.mkdir("SlackLogViewer");
	tmp.cd("SlackLogViewer");
	QString settingdir = tmp.absoluteDir() + "/";
	if (!tmp.exists("Cache")) tmp.mkdir("Cache");
	tmp.cd("Cache");
	gCacheDir = tmp.absolutePath() + "/";
#else
	QString settingdir = exedir.absolutePath() + "/";
	gResourceDir = exedir.absolutePath() + "/Resources/";
	if (!exedir.exists("Cache")) exedir.mkdir("Cache");
	exedir.cd("Cache");
	gCacheDir = exedir.absolutePath() + "/";
#endif

	gSettings = std::make_unique<QSettings>(settingdir + "settings.ini", QSettings::IniFormat);
	gSettings->setIniCodec(QTextCodec::codecForName("UTF-8"));
	Construct(*gSettings);

	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);


	QApplication a(argc, argv);
	QFont font(gSettings->value("Font/Family").toString(), gSettings->value("Font/Size").toInt());
	a.setFont(font);
	gDPI = std::make_unique<double>(QGuiApplication::primaryScreen()->physicalDotsPerInch());


	MainWindow::Construct();
	MainWindow::Get()->show();

	a.exec();

	//ここで明示的にDestroyしてSlackLogViewerインスタンスを削除しておかないと、QApplication::quit()でエラーになる。
	//staticなQApplicationよりも先にMainWindowが消滅していなければならないのだろうか。
	MainWindow::Destroy();
	return 0;
}
