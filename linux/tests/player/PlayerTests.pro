QT += core gui testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = shadowless-player-tests
SOURCES += tst_player.cpp ../../player/media_tools.cpp
HEADERS += ../../player/media_tools.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
