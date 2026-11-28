# Saved states: 0.1.8

Each file is the plugin's CLAP state as the 0.1.8 macOS arm64 build saved it, with the parameters set as listed below.
The build came from the published archive `0.1.8.olib` (sha256 `7f138fe574f353c517b82c93ec55179f821508cb6ffd48592dd04bbb460f5936`), whose macOS arm64 plugin is sha256 `8111f0f4d1592fda4affcea154166cb2de4e4ce1f42946c1ff1ea571900b6dca`.

| File | Parameters set from the default |
| --- | --- |
| `default.state` | none |
| `swing-octave.state` | Swing 30, Octave Range 1 |
| `hold-direct.state` | Hold On, Key Mode Direct, Trigger Toggle |

The states hold parameters only: no phrases and no assets.

The non-default files were saved with the host's plugin check, which reports one failed check on them: "the edit changes the sound". That check compares audio, and this note effect makes none, so the failure is the check's and not the state's. The parameters read back as set, and the tests in `tests/test_plugin.cpp` (the `saved_states` test) open each file with the current build.
