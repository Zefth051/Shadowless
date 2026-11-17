QT += core gui testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = tst_input_core
include(../../../overlays/input/core/input.pri)
SOURCES += tst_input_core.cpp
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
