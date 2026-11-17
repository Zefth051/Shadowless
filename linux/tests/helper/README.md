# Input setup helper tests

Build all three targets out of tree (the production helper has no Qt dependency;
only the test runner uses Qt Core/Test):

```sh
repo="$PWD"
build=$(mktemp -d /tmp/shadowless-input-tests.XXXXXX)
mkdir "$build/helper" "$build/tests" "$build/reload"
(cd "$build/helper" && qmake6 "$repo/linux/helper/input_setup/input_setup.pro" && make -j2)
(cd "$build/reload" && qmake6 "$repo/linux/tests/helper/fixture/ReloadFixture.pro" && make -j2)
(cd "$build/tests" && qmake6 "$repo/linux/tests/helper/HelperTests.pro" && make -j2)
export INPUT_SETUP_HELPER="$build/helper/shadowless-input-setup"
export INPUT_SETUP_RELOAD_HELPER="$build/reload/shadowless-input-setup-reload-fixture"
"$build/tests/shadowless-helper-tests"
unshare -r "$build/tests/shadowless-helper-tests"
```

All writes use private temporary roots. The production binary is exercised with
`--no-reload`. A separate binary links the same production source with test-only
`execve`/`write` wrappers, validating the command arguments and empty environment,
simulating reload/trigger and disk-write failures, and never executing udevadm.
The test runner rejects crashes, malformed JSON and multiline output.

## CLI contract

`shadowless-input-setup [--root DIR] [--no-reload] status|remove`

`shadowless-input-setup [--root DIR] [--no-reload] install VENDOR:PRODUCT:CLASS ...`

Options may also follow the subcommand. Install replaces the entire selection.
Supply 1–16 triples, with lowercase four-digit hex IDs and class
`keyboard`, `mouse`, or `gamepad`. The rule file contains the sorted unique triples. Other subcommands take no triples.
`--root` requires an absolute path and is rejected whenever `PKEXEC_UID` is
present, including an empty value; ordinary root in a user namespace is allowed.
The helper uses umask 022 and writes mode 0644. Symlinks in the target or its
parent path, nonregular files, hardlinks, and files outside its exact generated
format are refused. Status/remove on a missing file succeed.

Stdout contains exactly one JSON object and a newline:
`{"ok":true,"installed":true}` or
`{"ok":false,"installed":true,"error":"udev reload failed"}`.
`installed` reflects the observed file state, including successful mutation
before a reload failure. Validation errors occur before filesystem inspection.
Exit codes are 0 success, 2 invalid arguments, 3 I/O/unsafe target, 4 udev failure.

Status reads the managed file and includes its triples in a `devices` array.
When the file is absent, it returns `{"ok":true,"installed":false,"devices":[]}`.

Rules match the kernel input device's `id/vendor` and `id/product` attributes
and the event device's class flag. Keyboard, mouse, and gamepad use
`ID_INPUT_KEY`, `ID_INPUT_MOUSE`, and `ID_INPUT_JOYSTICK`, respectively.
After install or remove, the helper runs `udevadm control --reload-rules` and
then `udevadm trigger --action=change --subsystem-match=input`, unless invoked
with `--no-reload`. Repeating remove retries the reload even if the file is gone.

Live Polkit authorization and real device ACL changes are outside these tests.
