# Input overlay core (contract, layouts, renderer). Include from app and test projects.
QT += gui
INCLUDEPATH += $$PWD
HEADERS += $$PWD/input_types.h $$PWD/input_source.h $$PWD/fake_input_source.h $$PWD/layout.h $$PWD/input_renderer.h $$PWD/input_timeline.h
SOURCES += $$PWD/layout.cpp $$PWD/input_renderer.cpp $$PWD/input_timeline.cpp
RESOURCES += $$PWD/layouts/layouts.qrc
