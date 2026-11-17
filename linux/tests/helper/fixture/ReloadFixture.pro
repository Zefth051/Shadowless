TEMPLATE = app
CONFIG += console c++17
CONFIG -= qt app_bundle
TARGET = shadowless-input-setup-reload-fixture
SOURCES += ../../../helper/input_setup/main.cpp execve_fixture.cpp
QMAKE_CXXFLAGS += -Wall -Wextra -Werror
QMAKE_LFLAGS += -Wl,--wrap=execve -Wl,--wrap=write
