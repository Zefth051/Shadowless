# Windows package update validation — 2026-11-12

## Result

[Native package update QA run 43160914401](https://github.com/Zefth051/Shadowless/actions/runs/43160914401)
completed successfully on `windows-2022`, using source commit
`84c61d332f6108930315d6149c59b281ba32bd5f`.

The workflow built real private Setup and Portable packages at test versions
`9.8.6` and `9.8.7`. These are QA fixtures, not distributable Shadowless releases;
no release was created or published and repository visibility stayed private.

## Passed

- Validated the settings sentinel through production `Config.Normalize()`.
- Exercised production release selection and `PrepareAsync` for installed and
  Portable layouts, including checksum-file retrieval and real package preparation.
- Rejected an incorrect SHA-256 digest, removed partial download output, then
  successfully prepared the valid package again.
- Installed the old Setup, upgraded it with the prepared new Setup, and checked
  launcher version plus the updated runtime hash against the newly built payload.
- Preserved settings across the Setup upgrade.
- Ran the production Portable copy/relaunch helper on the old payload, checked
  the new version/runtime hash, and observed the updated native launcher starting.
- Preserved supported language, buffer duration, accent and replay-disabled
  settings across the Portable replacement/relaunch.

The ordinary [Windows Build run 43161028134](https://github.com/Zefth051/Shadowless/actions/runs/43161028134)
also passed at `ef964fbd4bac22d3bf5a51c850b88f69423f67a1`, including Release build,
the analyzer gate and existing public/private update-policy QA.

## Exact limits

Package downloads used an injected HTTP transport backed by the real local build
artifacts and GitHub-shaped URLs. This preserves production URL/checksum checks,
but does not contact a published update feed. The harness enables update code;
the packaged private application itself still has updates disabled.

Setup was invoked silently from the prepared path. The application's interactive
update button and `UpdateService.Launch` installer invocation were not exercised.
For Portable, a test wrapper observed the actual launcher process start; that
does not prove a healthy WPF/GPU session or capture pipeline. Recordings were not
used as preservation fixtures. SmartScreen/signing, Windows 10 coverage and a
complete upgrade through two published GitHub releases remain unvalidated.

## Reproduce and evidence

Dispatch **Native package update QA** on `windows`:

```sh
gh workflow run update-qa.yml --repo Zefth051/Shadowless --ref windows
```

It builds its own two private test packages and creates no GitHub release.
The workflow log records all assertions and limits. The run's
`native-update-qa-evidence` artifact contains installer/helper logs and preparation
results, retained for seven days. Raw runner logs and local test paths are not
copied into this source repository.

The production updater now consistently uses its existing HTTP client for
discovery, checksums and package downloads, allowing this package path to be
tested without embedding private repository credentials.
See [updates and local development](UPDATES_AND_LOCAL_WORK.md) and
[current release status](RELEASE_STATUS.md) before distributing a candidate.
