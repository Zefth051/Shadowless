QT = core testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = shadowless-helper-tests
SOURCES += tst_helper.cpp
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
