QT += core network testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = captail-gsr-tests
SOURCES += tst_gsr.cpp ../capture/gsr_capture.cpp
HEADERS += ../capture/gsr_capture.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
