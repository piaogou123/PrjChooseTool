#-------------------------------------------------------------------
# PrjChooseTool - select a Data_User variant per project and update
# Data_System/ProjectDefinition.dat accordingly.
#
# Builds with both Qt 5 and Qt 6 (qmake project).
# Open this .pro file in Qt Creator, then Build & Run.
#-------------------------------------------------------------------

QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
CONFIG -= app_bundle

TARGET   = PrjChooseTool
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

RESOURCES += \
    app.qrc

# Windows executable icon.
win32: RC_ICONS = appicon.ico
