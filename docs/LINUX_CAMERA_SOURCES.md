# Linux camera sources

Phase 1 is a QtCore-only, read-only sysfs catalog in
`linux/capture/camera/camera_catalog.{h,cpp}`. It has no UI or capture integration.
The catalog reads attributes under `/sys/class/video4linux`, resolves symlinks,
and uses `stat(2)` and `access(2)` for `/dev/video<N>` nodes. It never opens a
video node, issues an ioctl, captures frames, or changes permissions or ownership.

## Source taxonomy

| Source | Phase-1 interpretation | Verification boundary |
| --- | --- | --- |
| UVC webcams | `uvcvideo` and USB paths identify physical candidates. | Real cameras and their supported formats are UNVERIFIED. |
| v4l2loopback bridges | `v4l2loopback` driver or known bridge names identify virtual candidates. | A loaded loopback node does not prove a producer exists. |
| DroidCam | Case-insensitive name hint `DroidCam`. | Installed versions, driver variants, connection methods, and live integration are UNVERIFIED. |
| scrcpy `--v4l2-sink` | Case-insensitive name hint `scrcpy` when present; otherwise a loopback driver can yield a generic hint. | Command availability, setup, node naming, and live integration are UNVERIFIED. |
| OBS Virtual Camera | Case-insensitive name hint `OBS Virtual Camera` when present; otherwise a generic loopback hint may apply. | OBS installation, loopback configuration, naming, and live integration are UNVERIFIED. |
| Camo-style applications exposing a virtual camera | A V4L2 node can appear as a generic candidate; no dedicated Camo name heuristic exists. | Linux support and the backend used by any particular application are UNVERIFIED. |
| PipeWire / portal cameras | Later option, outside this sysfs catalog. | Portal API, desktop availability, consent flow, and interoperability are UNVERIFIED. |

Names are hints, not authenticated identities. A V4L2 entry can represent a
non-camera function. Physical classification uses `uvcvideo` or USB/PCI/platform
paths; it does not validate V4L2 capture capabilities. Unknown devices remain
visible. Absence from sysfs does not prove absence from PipeWire or a portal.

## Detected versus likely usable

`detected = true` means an entry directory or directory symlink named
`video<N>` was listed by sysfs, even if attributes or its device node are missing.
Results use numeric ordering (`video2` before `video10`). `id` is the sysfs name;
it is stable for that entry, **not a persistent hardware identity**. Persistence
across unplugging, reboot, or enumeration changes is UNVERIFIED.

The conservative rule is that `likelyUsable()` is true only for
`Usability::LikelyUsable`. Classification precedence is:

1. `NodeMissing`: the path is missing or is not a character device.
2. `PermissionDenied`: `access(path, R_OK | W_OK)` fails. This is an access-check
   result, not a precise errno diagnosis; removal races can also cause failure.
3. `SecondaryNode`: index is greater than zero and an index-0 sibling has the
   same nonempty resolved `device` target. UVC metadata nodes motivate this
   heuristic; it does not prove the node carries metadata. A sibling need only
   exist in sysfs, even if that sibling has no accessible device node.
4. `NoProducerUnknown`: an otherwise accessible loopback candidate. The reason
   explains that a producer cannot be verified without opening the device.
   `likelyUsable()` is **false** for these candidates.
5. `Unknown`: the index is missing, negative, overflowing, or malformed.
6. `LikelyUsable`: an accessible character device with a valid index, without
   the preceding exclusions. Its reason still states that capture is unverified.

The catalog cannot prove capture support, producer activity, format compatibility,
frame delivery, device availability, or exclusive access without opening and
probing. Discovery is a snapshot; permissions and devices can change immediately
following a scan. No successful scan is permission to capture.

Each probe callback is independently injectable for tests. Defaults use `stat`,
`access`, and symlink resolution. Tests can use ordinary files as fake nodes
without root or hardware. Malformed or missing sysfs attributes produce safe
empty/unknown values. Attribute reads are bounded to 4096 bytes; names have
Unicode control/format characters removed, are trimmed, and are limited to 80
UTF-16 code units without leaving a trailing unmatched high surrogate.

## Permissions and privacy

The USER or system administrator manages camera access through the distribution's
`video` group or appropriate udev `uaccess` rules for the active session.
Exact rules and group policy vary by distribution and are UNVERIFIED here.
Group membership may grant access beyond a single camera. Shadowless never runs
permission repair, changes groups, invokes sudo, or changes ownership/mode bits.

This module captures no images or audio, starts no bridge processes, and logs no
device names. Names are returned in memory for a future explicit source picker;
they must not enter diagnostics or telemetry. The module itself has no logging.

## Phases

1. **Catalog:** read-only discovery and conservative status reporting (this module).
2. **Explicit user-confirmed open/format probe:** a separate module may open only
   the selected source after consent, query capture capabilities and formats,
   and explain errors. This phase is not implemented.
3. **Capture/composition:** opt-in frame acquisition and composition, with clear
   lifecycle and stop/release behavior. This phase is not implemented.

## Open questions

- Which real cameras and bridge versions should be validated first? All live
  capture and bridge interoperability remain UNVERIFIED.
- Should additional secondary nodes be selectable after explicit capability
  probing when the phase-1 metadata heuristic is too broad?
- Which persistent identity should a future source picker store across reboots?
- Which formats, frame rates, and conversion paths will composition accept?
- How should hotplug, permission changes, busy devices, and producer loss appear
  in the UI? The catalog currently offers synchronous snapshots only.
- Should portal/PipeWire camera access precede direct V4L2 on supported desktops?
- Who registers the standalone test project in the shared Linux build? This task
  intentionally leaves `linux/Shadowless.pro` and build registration unchanged.

## Validation

Build outside the checkout:

```sh
mkdir -p /tmp/build-t4
cd /tmp/build-t4
qmake6 /ABS/linux/tests/camera/CameraCatalogTests.pro
make -j8
QT_QPA_PLATFORM=offscreen ./tst_camera_catalog
```

The suite uses fake sysfs/device trees, real symlinks, and injected probes. It
also scans the real `/sys/class/video4linux` without opt-in, asserting only
invariants; an empty result is valid and is not hardware validation. Compilation
uses `-Wall -Wextra -Werror` and links QtCore/QtTest without QtGui.
