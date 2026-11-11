QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TARGET = shadowless-desktop-entry-check
SOURCES += check.cpp ../../app/desktop_entry.cpp
RESOURCES += ../../app/resources.qrc
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
