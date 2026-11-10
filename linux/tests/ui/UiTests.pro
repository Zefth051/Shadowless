QT += core gui widgets network testlib dbus concurrent
CONFIG += console testcase c++17 link_pkgconfig
CONFIG -= app_bundle
TARGET = shadowless-ui-tests
PKGCONFIG += libavformat libavcodec libavutil libswscale
SOURCES += ../tst_ui.cpp ../../ui/dashboard.cpp ../../ui/library.cpp ../../ui/settings_page.cpp ../../ui/overlay_editor.cpp ../../ui/theme.cpp \
    ../../overlays/layer.cpp ../../overlays/media.cpp ../../capture/gsr_capture.cpp ../../capture/audio.cpp ../../platform/hyprland/bindings.cpp \
    ../../app/controller.cpp ../../settings/settings.cpp ../../overlays/overlay_client.cpp \
    ../../platform/hyprland/overlay_surface.cpp ../../platform/hyprland/shortcuts.cpp
HEADERS += ../../ui/dashboard.h ../../ui/library.h ../../ui/settings_page.h ../../ui/overlay_editor.h ../../ui/theme.h \
    ../../overlays/layer.h ../../overlays/media.h ../../capture/gsr_capture.h ../../capture/audio.h ../../platform/hyprland/bindings.h \
    ../../app/controller.h ../../settings/settings.h ../../overlays/overlay_client.h \
    ../../platform/hyprland/overlay_surface.h ../../platform/hyprland/shortcuts.h
LAYERSHELL_PREFIX = $$(SHADOWLESS_LAYERSHELL_PREFIX)
!isEmpty(LAYERSHELL_PREFIX) {
    INCLUDEPATH += $$LAYERSHELL_PREFIX/include
    LIBS += -L$$LAYERSHELL_PREFIX/lib
    QMAKE_RPATHDIR += $$LAYERSHELL_PREFIX/lib
}
LIBS += -lLayerShellQtInterface
RESOURCES += ../../app/resources.qrc
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
