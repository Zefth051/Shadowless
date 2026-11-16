QT += core gui network dbus testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = shadowless-shortcut-tests
SOURCES += tst_shortcuts.cpp ../../platform/hyprland/shortcuts.cpp ../../platform/hyprland/bindings.cpp
HEADERS += ../../platform/hyprland/shortcuts.h ../../platform/hyprland/bindings.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
