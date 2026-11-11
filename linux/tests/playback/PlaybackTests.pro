QT += core gui widgets openglwidgets testlib
CONFIG += console testcase c++17 link_pkgconfig
CONFIG -= app_bundle
TARGET = shadowless-playback-tests
PKGCONFIG += mpv
SOURCES += tst_playback.cpp ../../player/mpv_player.cpp
HEADERS += ../../player/mpv_player.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
