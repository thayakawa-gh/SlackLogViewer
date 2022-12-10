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
	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
	QApplication a(argc, argv);

	QDir exedir = QCoreApplication::applicationDirPath();
	QString settingdir = exedir.absolutePath() + "/";
	gSettings = std::make_unique<QSettings>(settingdir + "settings.ini", QSettings::IniFormat);
	gSettings->setIniCodec(QTextCodec::codecForName("UTF-8"));
	Construct(*gSettings, exedir);

#ifdef __APPLE__
	QDir tmp = exedir;
	tmp.cdUp();
	gResourceDir = tmp.absolutePath() + "/Resources/";
#else
	gResourceDir = exedir.absolutePath() + "/Resources/";
#endif
	gCacheDir = gSettings->value("Cache/Location").toString() + "/";

	QFont font(gSettings->value("Font/Family").toString(), gSettings->value("Font/Size").toInt());
	a.setFont(font);
	QPalette p = QApplication::palette();
	p.setColor(QPalette::Text, Qt::black);
	QApplication::setPalette(p);
	gDPI = std::make_unique<double>(QGuiApplication::primaryScreen()->physicalDotsPerInch());

	MainWindow::Construct();
	MainWindow::Get()->show();

	a.exec();

	//ここで明示的にDestroyしてSlackLogViewerインスタンスを削除しておかないと、QApplication::quit()でエラーになる。
	//staticなQApplicationよりも先にMainWindowが消滅していなければならないのだろうか。
	MainWindow::Destroy();
	return 0;
}
