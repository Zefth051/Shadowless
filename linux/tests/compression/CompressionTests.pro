QT += core concurrent testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = shadowless-compression-tests
SOURCES += tst_compression.cpp ../../app/idle_compressor.cpp
HEADERS += ../../app/idle_compressor.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
