QT += core gui widgets network
CONFIG += c++17
TARGET = shadowless-capture-fixture
SOURCES += ../capture_fixture.cpp ../../settings/settings.cpp ../../overlays/layer.cpp
HEADERS += ../../settings/settings.h ../../overlays/layer.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
