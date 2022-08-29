#include "SlackLogViewer.h"
#include "GlobalVariables.h"
#include <QSettings>
#include <QtWidgets/QApplication>
#include <QTextCodec>
#include <QGuiApplication>
#include <QScreen>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <unistd.h>
#endif

int main(int argc, char *argv[])
{
	gSettings = std::make_unique<QSettings>("settings.ini", QSettings::IniFormat);
	gSettings->setIniCodec(QTextCodec::codecForName("UTF-8"));
	Construct(*gSettings);

	QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
	QApplication a(argc, argv);
	QFont font(gSettings->value("Font/Family").toString(), gSettings->value("Font/Size").toInt());
	a.setFont(font);
	gDPI = std::make_unique<double>(QGuiApplication::primaryScreen()->physicalDotsPerInch());

#ifdef __APPLE__
	std::string path;
	uint32_t bufsize=0;
	_NSGetExecutablePath((char*)path.c_str(), &bufsize);
	path.resize(bufsize+1);
	_NSGetExecutablePath((char*)path.c_str(), &bufsize);
	auto idx=path.rfind('/');
	path=path.substr(0,idx);
	chdir(path.c_str());
#endif

	MainWindow::Construct();
	MainWindow::Get()->show();

	a.exec();

	//ここで明示的にDestroyしてSlackLogViewerインスタンスを削除しておかないと、QApplication::quit()でエラーになる。
	//staticなQApplicationよりも先にMainWindowが消滅していなければならないのだろうか。
	MainWindow::Destroy();
	return 0;
}
