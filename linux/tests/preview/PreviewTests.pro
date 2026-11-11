QT += core gui widgets network testlib
CONFIG += console testcase c++17 link_pkgconfig
CONFIG -= app_bundle
TARGET = shadowless-preview-tests
PKGCONFIG += libavformat libavcodec libavutil libswscale
SOURCES += tst_preview.cpp ../../overlays/preview_client.cpp ../../plugins/compositor.cpp ../../overlays/layer.cpp ../../overlays/media.cpp ../../ui/overlay_editor.cpp
HEADERS += ../../overlays/preview_client.h ../../plugins/compositor.h ../../overlays/layer.h ../../overlays/media.h ../../ui/overlay_editor.h
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
