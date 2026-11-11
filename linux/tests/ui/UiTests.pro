QT += multimedia multimediawidgets core gui widgets network testlib dbus concurrent openglwidgets
CONFIG += console testcase c++17 link_pkgconfig
CONFIG -= app_bundle
TARGET = shadowless-ui-tests
SOURCES += ../../app/update_service.cpp
HEADERS += ../../app/update_service.h
PKGCONFIG += libavformat libavcodec libavutil libswscale
PKGCONFIG += mpv
SOURCES += ../../player/mpv_player.cpp ../../player/media_tools.cpp ../../overlays/preview_client.cpp
HEADERS += ../../player/mpv_player.h ../../player/media_tools.h ../../overlays/preview_client.h
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

SOURCES += ../../ui/status_indicator.cpp
HEADERS += ../../ui/status_indicator.h

SOURCES += ../../ui/clip_player.cpp
HEADERS += ../../ui/clip_player.h

SOURCES += ../../capture/replay_paths.cpp ../../ui/clip_notification.cpp
HEADERS += ../../capture/replay_paths.h ../../ui/clip_notification.h
