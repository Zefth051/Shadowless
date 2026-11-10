# Hyprland overlay validation

The native preview was built on Hyprland 0.56.2 with Qt 6.12.0,
LayerShellQt 6.8.2 and GPU Screen Recorder 6.1.5. The hardware capture
checks used NVIDIA NVENC H.264 at 1920 × 1080 and 60 FPS on DP-1.

The dashboard mapped above a fullscreen native Wayland fixture. The
compositor action opened it, Escape hid it, and the fixture received
keyboard input afterward. The Alt+Z, Ctrl+Shift+F10 and Alt+F10 bindings
registered in the Lua compositor. The test's virtual keyboard did not
activate Alt+Z. Physical keyboard and real-game checks remain open.
See the [Hyprland shortcut documentation](https://wiki.hypr.land/configuring/core/binds/globals/).

The application test used an isolated configuration with one red layer
and the experimental indicator enabled. The indicator mapped during
capture. A save immediately followed by Quit produced a replay that
decoded fully, with a sample of RGB (248, 0, 0) at the layer's expected
position. The static layer uploaded once. The preview's default keeps the
indicator disabled. Indicator GPU cost and pointer behavior in a real
game have not been measured.

The three offline suites reported 31 passes and zero failures. The two
pre-existing live capture cases were skipped. The separate composition
run produced four replays and a continuous recording. FFmpeg decoded all
five files. The fixtures checked static layer position and orientation
with composition enabled.

| Configuration | Recorder CPU, % of one core | Recorder RSS, MiB | Mean CPU time per composition call |
| --- | ---: | ---: | ---: |
| GSR baseline | 4.5 | 360 | — |
| Plugin loaded, composition disabled | 5.0 | 369 | — |
| Static image | 5.5 | 376 | 13 µs |
| Static image and animated video | 7.0 | 402 | 39 µs |
| Continuous recording with both layers | 6.75 | 402 | 39 µs |

Each row is one four-second sample from the same desktop session. CPU
measures the recorder process, including its decoding thread. The last
column measures time spent submitting composition on the CPU. Game FPS,
GPU execution time and long-session behavior require separate measurements.
Raw logs and clips are stored locally under
`.qa/hyprland-overlay/perf-final/` and `.qa/hyprland-overlay/controller-clips/`.

GSR 6.1.5 produced external-texture errors when its plugin pipeline tried
to draw certain hardware cursor textures. The application excludes the
mouse cursor from recordings made with the composition plugin. The five
final capture cases completed without those errors.
