TEMPLATE = subdirs
SUBDIRS += updatetests
updatetests.file = tests/updates/UpdateTests.pro
SUBDIRS += portaltests
portaltests.file = tests/shortcuts/ShortcutTests.pro
SUBDIRS += plugins app tests overlaytests uitests playertests previewtests desktoptests playbacktests
playbacktests.file = tests/playback/PlaybackTests.pro
playertests.file = tests/player/PlayerTests.pro
previewtests.file = tests/preview/PreviewTests.pro
desktoptests.file = tests/desktop_entry/DesktopEntryTests.pro
plugins.file = plugins/ShadowlessOverlay.pro
app.depends = plugins
app.file = app/ShadowlessLinux.pro
tests.file = tests/GsrTests.pro
overlaytests.file = tests/overlays/OverlayTests.pro
uitests.file = tests/ui/UiTests.pro

SUBDIRS += compressiontests
compressiontests.file = tests/compression/CompressionTests.pro
SUBDIRS += discordtests
discordtests.file = tests/discord/DiscordTests.pro

# Input overlay suites (contract/layouts, evdev source, recorder, sidecar session)
SUBDIRS += inputcoretests inputevdevtests inputrecordertests inputsidecartests cameratests
inputcoretests.file = tests/input/core/InputCoreTests.pro
inputevdevtests.file = tests/input/evdev/EvdevTests.pro
inputrecordertests.file = tests/input/recorder/RecorderTests.pro
inputsidecartests.file = tests/input/sidecar/SidecarTests.pro

# Read-only V4L2 camera catalog
cameratests.file = tests/camera/CameraCatalogTests.pro
