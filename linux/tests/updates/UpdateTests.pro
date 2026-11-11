QT += core network testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = shadowless-update-tests
SOURCES += tst_updates.cpp ../../app/update_service.cpp
HEADERS += ../../app/update_service.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
