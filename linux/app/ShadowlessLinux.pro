QT += multimedia multimediawidgets core gui widgets network dbus concurrent openglwidgets
CONFIG += c++17
CONFIG -= app_bundle
TARGET = shadowless
SHADOWLESS_VERSION = $$(SHADOWLESS_VERSION)
isEmpty(SHADOWLESS_VERSION): SHADOWLESS_VERSION = 0.3.2
DEFINES += SHADOWLESS_VERSION=\\\"$$SHADOWLESS_VERSION\\\"
equals(SHADOWLESS_PUBLIC_UPDATES, true): DEFINES += SHADOWLESS_PUBLIC_UPDATES
SOURCES += update_service.cpp
HEADERS += update_service.h
SOURCES += main.cpp controller.cpp desktop_entry.cpp ../capture/gsr_capture.cpp ../capture/audio.cpp \
    ../settings/settings.cpp ../overlays/layer.cpp ../overlays/media.cpp ../overlays/overlay_client.cpp \
    ../platform/hyprland/overlay_surface.cpp ../platform/hyprland/shortcuts.cpp ../platform/hyprland/bindings.cpp \
    ../ui/theme.cpp ../ui/library.cpp ../ui/settings_page.cpp ../ui/overlay_editor.cpp ../ui/dashboard.cpp
HEADERS += controller.h desktop_entry.h ../capture/gsr_capture.h ../capture/audio.h ../settings/settings.h \
    ../overlays/layer.h ../overlays/media.h ../overlays/overlay_client.h \
    ../platform/hyprland/overlay_surface.h ../platform/hyprland/shortcuts.h ../platform/hyprland/bindings.h \
    ../ui/theme.h ../ui/library.h ../ui/settings_page.h ../ui/overlay_editor.h ../ui/dashboard.h
CONFIG += link_pkgconfig
PKGCONFIG += libavformat libavcodec libavutil libswscale
PKGCONFIG += mpv
SOURCES += ../player/mpv_player.cpp ../player/media_tools.cpp ../overlays/preview_client.cpp
HEADERS += ../player/mpv_player.h ../player/media_tools.h ../overlays/preview_client.h
LAYERSHELL_PREFIX = $$(SHADOWLESS_LAYERSHELL_PREFIX)
!isEmpty(LAYERSHELL_PREFIX) {
    INCLUDEPATH += $$LAYERSHELL_PREFIX/include
    LIBS += -L$$LAYERSHELL_PREFIX/lib
    QMAKE_RPATHDIR += $$LAYERSHELL_PREFIX/lib
}
LIBS += -lLayerShellQtInterface
RESOURCES += resources.qrc
QMAKE_CXXFLAGS += -Wall -Wextra -Werror

SOURCES += ../ui/status_indicator.cpp
HEADERS += ../ui/status_indicator.h

SOURCES += ../ui/clip_player.cpp
HEADERS += ../ui/clip_player.h

SOURCES += ../capture/replay_paths.cpp ../ui/clip_notification.cpp
HEADERS += ../capture/replay_paths.h ../ui/clip_notification.h
