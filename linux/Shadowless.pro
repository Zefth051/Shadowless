TEMPLATE = subdirs
SUBDIRS = plugins app tests overlaytests uitests
plugins.file = plugins/ShadowlessOverlay.pro
app.depends = plugins
app.file = app/ShadowlessLinux.pro
tests.file = tests/GsrTests.pro
overlaytests.file = tests/overlays/OverlayTests.pro
uitests.file = tests/ui/UiTests.pro
