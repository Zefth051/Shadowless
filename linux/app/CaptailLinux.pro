QT += core gui widgets network
CONFIG += c++17
CONFIG -= app_bundle
TARGET = captail-linux
SOURCES += main.cpp ../capture/gsr_capture.cpp
HEADERS += ../capture/gsr_capture.h
RESOURCES += resources.qrc
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
