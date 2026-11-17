QT += core testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = tst_camera_catalog
SOURCES += tst_camera_catalog.cpp ../../capture/camera/camera_catalog.cpp
HEADERS += ../../capture/camera/camera_catalog.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
