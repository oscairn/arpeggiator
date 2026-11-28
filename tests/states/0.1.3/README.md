# Saved states: 0.1.3

Each file is the plugin's CLAP state as the 0.1.3 macOS arm64 build saved it, with the parameters set as listed below.
The build came from the published archive `0.1.3.olib` (sha256 `9385f74083c02e5dc053e95b0b71d5d57a86d3bf056e091ce09fa327c2d61bba`), whose macOS arm64 plugin is sha256 `74df76bfd25a3b210151c76406959b5b3311850da158898db044227b292cf8af`.

| File | Parameters set from the default |
| --- | --- |
| `default.state` | none |
| `swing-octave.state` | Swing 30, Octave Range 1 |
| `hold-direct.state` | Hold On, Key Mode Direct, Trigger Toggle |

The states hold parameters only: no phrases and no assets.

The non-default files were saved with the host's plugin check, which reports one failed check on them: "the edit changes the sound". That check compares audio, and this note effect makes none, so the failure is the check's and not the state's. The parameters read back as set, and the tests in `tests/test_plugin.cpp` (the `saved_states` test) open each file with the current build.
