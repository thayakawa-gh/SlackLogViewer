#include "SlackLogViewer.h"
#include "GlobalVariables.h"
#include <QSettings>
#include <QtWidgets/QApplication>
#include <QTextCodec>
#include <QGuiApplication>
#include <QScreen>
#include <Windows.h>

int main(int argc, char *argv[])
{
	SetErrorMode(0);

	gSettings = std::make_unique<QSettings>("settings.ini", QSettings::IniFormat);
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
