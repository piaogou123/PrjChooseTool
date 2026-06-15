/*
 * @CopyRight: iNovatrol
 * @Description: Application entry point for PrjChooseTool
 * @version: <SET-YOUR-INITIALS> <SET-YOUR-VERSION>   // e.g. JCK J01
 * @Author: <SET-YOUR-NAME-IN-PINYIN>
 * @Date: 2026.06.15
 */

#include "mainwindow.h"   // HD-10: own header first

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    // Organization + application name are required so QSettings can persist
    // the last-used working folder between launches.
    app.setOrganizationName("iNovatrol");
    app.setApplicationName("PrjChooseTool");

    prjchoosetool::MainWindow window;
    window.show();

    return app.exec();
}
