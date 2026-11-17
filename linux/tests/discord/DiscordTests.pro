QT += core network testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = shadowless-discord-tests
SOURCES += tst_discord.cpp ../../app/discord_presence.cpp
HEADERS += ../../app/discord_presence.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
