QT += core network testlib concurrent
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = shadowless-gsr-tests
SOURCES += tst_gsr.cpp ../capture/gsr_capture.cpp ../capture/audio.cpp
HEADERS += ../capture/gsr_capture.h ../capture/audio.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
