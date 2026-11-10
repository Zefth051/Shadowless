QT += core gui widgets network dbus concurrent
CONFIG += c++17
CONFIG -= app_bundle
TARGET = shadowless
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
LAYERSHELL_PREFIX = $$(SHADOWLESS_LAYERSHELL_PREFIX)
!isEmpty(LAYERSHELL_PREFIX) {
    INCLUDEPATH += $$LAYERSHELL_PREFIX/include
    LIBS += -L$$LAYERSHELL_PREFIX/lib
    QMAKE_RPATHDIR += $$LAYERSHELL_PREFIX/lib
}
LIBS += -lLayerShellQtInterface
RESOURCES += resources.qrc
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
