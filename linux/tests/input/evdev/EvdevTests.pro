QT += core testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = tst_evdev_source
include(../../../overlays/input/source/evdev.pri)
SOURCES += tst_evdev_source.cpp
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
QMAKE_LFLAGS += -Wl,--wrap=ioctl
