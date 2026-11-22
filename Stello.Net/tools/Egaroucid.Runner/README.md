# Egaroucid Runner

Standalone native C++ driver for `Egaroucid-main/src/engine` (`© Takuto Yamana, GPL-3.0-or-later`), built without modifying `Egaroucid-main/` and spoken over stdin/stdout by `Stello.Benchmark` (`engine=egaroucid-cpp`).

## Build (Windows, Clang/LLVM)

```cmd
Stello.Net\tools\Egaroucid.Runner\build.cmd
```

Produces `Stello.Net/tools/Egaroucid.Runner/bin/egaroucid-runner.exe` (git-ignored). Set `EGAROUCID_RUNNER` to override its path.

## Protocol

- `init <threads> <hashBits> <resourceDir>` → `ready`
- `clear` → `ok`
- `go <ownHex> <oppHex> movetime <ms> | gametime <ms> | level <N> | depth <D> <mpcIdx> | solve` → `bestmove <sq> score <v> depth <d> prob <p> end <0|1> nodes <n> ms <t>`
- `quit`
