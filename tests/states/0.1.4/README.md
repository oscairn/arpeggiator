# Saved states: 0.1.4

Each file is the plugin's CLAP state as the 0.1.4 macOS arm64 build saved it, with the parameters set as listed below.
The build came from the published archive `0.1.4.olib` (sha256 `d0c47096707a1d5362628f48eebee88017095115e9b74f60f0221fec1f5d0d16`), whose macOS arm64 plugin is sha256 `d4df14c6734b94b95d6f49c0444c5c573b3d5e573610d9ca484e15cd30b39377`.

| File | Parameters set from the default |
| --- | --- |
| `default.state` | none |
| `swing-octave.state` | Swing 30, Octave Range 1 |
| `hold-direct.state` | Hold On, Key Mode Direct, Trigger Toggle |

The states hold parameters only: no phrases and no assets.

The non-default files were saved with the host's plugin check, which reports one failed check on them: "the edit changes the sound". That check compares audio, and this note effect makes none, so the failure is the check's and not the state's. The parameters read back as set, and the tests in `tests/test_plugin.cpp` (the `saved_states` test) open each file with the current build.
