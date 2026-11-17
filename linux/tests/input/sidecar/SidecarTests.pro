QT += core testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = tst_input_sidecar
CORE = ../../../overlays/input/core
SOURCE = ../../../overlays/input/source
INCLUDEPATH += $$CORE $$SOURCE
HEADERS += $$CORE/input_source.h $$CORE/fake_input_source.h $$SOURCE/input_recorder.h $$SOURCE/input_sidecar_session.h
SOURCES += tst_input_sidecar.cpp $$SOURCE/input_sidecar_session.cpp $$SOURCE/input_recorder.cpp \
           $$CORE/layout.cpp $$CORE/input_timeline.cpp
RESOURCES += $$CORE/layouts/layouts.qrc
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
