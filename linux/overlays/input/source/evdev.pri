QT += core
INCLUDEPATH += $$PWD
HEADERS += $$PWD/../core/input_types.h $$PWD/../core/input_source.h \
           $$PWD/evdev_source.h $$PWD/evdev_keymap.h
SOURCES += $$PWD/evdev_source.cpp $$PWD/evdev_keymap.cpp
