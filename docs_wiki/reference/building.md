---
type: reference
title: "Building This Fork (Windows)"
description: "Windows build procedure for the pyareas fork: environment, one-time setup, build commands, timings, and gotchas"
tags: [build, windows, cmake, msvc]
last_updated: 2026-09-12
---

# Building This Fork (Windows)

Tests verified this procedure on 2026-07-31 against `main` at commit `027ef661`
(Blender 5.3.0 alpha) on Windows 10.

The official documentation at
<https://developer.blender.org/docs/handbook/building_blender/windows/>
describes a `make.bat` workflow. This workflow does not work unattended on
this machine. See [Gotchas](#gotchas) for the reasons. Use the commands in
this document instead.

## Environment

| Component | Path or version on this machine |
| :--- | :--- |
| Source | `<repo>` |
| Build dir | `<build>` |
| Visual Studio | 2022 Enterprise, MSVC 14.36.32532 (`MSVC_VERSION` 1936) |
| CMake | 3.26.0-msvc3, VS-bundled only, not on system PATH |
| OptiX SDK | `C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.1.0`, optional, see [OptiX is not needed in this fork](#optix-is-not-needed-in-this-fork) |
| Libraries | `lib/windows_x64` at commit `60d6e96b` (tag `v5.2.0`), 6.5 GB |
| git-lfs | 3.6.1, required for the libraries |
| Python | 3.12.0, required to run `make_update.py` directly |

CMake is at this path:

```
C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

The version requirements have room to spare. The tree needs CMake 3.21 or
later (`CMakeLists.txt:29`) and MSVC 1928 or later (`CMakeLists.txt:131`).

## One-time setup

### Clone

Run this command to clone the repository:

```powershell
git clone --branch main https://projects.blender.org/blender/blender.git <repo>
```

`main` is the alpha branch. Do not use an LTS tag. The path must not contain
spaces.

### Libraries

Do not use `make.bat update`. It prompts "Would you like to download them?
(y/n)" and a non-interactive run answers no, then exits with code 1. Call the
updater directly instead:

```powershell
cd <repo>
python .\build_files\utils\make_update.py --no-blender
```

The `--no-blender` flag skips the source-repo pull. It does not rebase or
fast-forward your working branch. It still fetches the libraries, which is
the only part that matters.

Expect about 6.5 GB of download and a long
stretch during `git lfs pull`. That step produces no incremental output and
looks frozen when it works normally.

The submodule state afterward must look like this. The three non-Windows
entries are meant to stay uninitialized:

```
-ecbd06cf... lib/linux_x64
-a76ef917... lib/macos_arm64
-c65cd3db... lib/windows_arm64
 60d6e96b... lib/windows_x64 (v5.2.0)
```

There are no add-on or test submodules to initialize in 5.3. Add-ons live in
the repository itself, under `scripts/`.

### Configure

Run this step once. It takes about 20 seconds.

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S "<repo>" `
         -B "<build>" `
         -G "Visual Studio 17 2022" -A x64 `
         -DOPTIX_ROOT_DIR="C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.1.0"
```

A final line reading `"-- Build files have been written to: ..."` indicates success.

Do not reconfigure from scratch afterward. CMake re-runs itself automatically
when `CMakeLists.txt` files change. A manual wipe costs a full rebuild for no
reason.

Never reconfigure while a build runs. Changing cache variables
invalidates targets that MSBuild already walks.

### OptiX is not needed in this fork

The previous fork modified the Cycles kernel and introduced the `-DOPTIX_ROOT_DIR=...` flag.
This fork touches only the UI and editor layer,
so the build does not require OptiX.

Its cost is small either way. The configure summary reports
`WITH_CYCLES_CUDA_BINARIES OFF` and `WITH_CYCLES_HIP_BINARIES OFF`, so the
build compiles no GPU kernels.

OptiX contributes only host-side device code
and headers. Dropping it saves minutes, not hours.

To build without it, omit `-DOPTIX_ROOT_DIR` and add this flag:

```
-DWITH_CYCLES_DEVICE_OPTIX=OFF
```

If iteration speed matters more than release fidelity, drop Cycles wholesale
with `-DWITH_CYCLES=OFF`, because nothing in this fork's plan touches
rendering.

Build the final, publishable version with the full default
configuration, so the binary matches what upstream users would compile.

## Building

Use this same command for the first build and for every rebuild:

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$build = "<build>"
& $cmake --build $build --config Release --target INSTALL -- /m
```

- The `--target INSTALL` flag is required. Building the default target
  leaves DLLs uncopied, and the resulting `blender.exe` does not launch.

- The `--target INSTALL` flag is equally required after Python-only changes.
  That failure is silent rather than obvious. The build copies `scripts/`
  into `bin/Release/<version>/scripts/` at install time.

  Building only the `blender` target leaves Blender running whatever copy of
  `scripts/startup/bl_ui/*.py` the build installed last. Edits to `space_addon.py`
  then have no effect at all, with no error. The panel simply never
  registers.

  This cost a long debugging detour on 2026-08-18, chasing C++
  causes for a stale 13-day-old script. If a Python change appears to do
  nothing, check the timestamp of the installed copy before anything else.

- The `/m` flag enables parallel MSBuild.

The output binary is at this path:

```
<build>\bin\Release\blender.exe
```

### Timings

Measured on 2026-08-01 at `/m:6` on the machine described in
[Environment](#environment) (16 cores, 32 GB).

| Build | Duration |
| :--- | :--- |
| First full build | 40 to 90 minutes |
| Incremental, one editor `.cc` file (`space_addon.cc`) | 1 minute 7 seconds |
| Incremental, Cycles-only change | About 2 minutes |
| Incremental, DNA header touched | 8 minutes 49 seconds |

Touching the file, not changing its layout, triggers the DNA cascade.
The dependency is a timestamp, so a comment-only edit to a `DNA_*.h`
file costs the same 8-minute rebuild as adding a struct member.

The `makesdna` tool re-runs, and everything downstream of the generated DNA
rebuilds, even when the generated output is byte-identical. Batch DNA edits
together rather than making them one at a time.

By contrast, work confined to `source/blender/editors/space_addon/`,
including its own `addon_intern.hh` (which is not a DNA header), recompiles
one translation unit and relinks in about a minute.

Runtime-only structs such
as `SpaceAddon_Runtime` live there deliberately, because only the opaque
pointer to it is in DNA. Changes to the runtime cache, panel collection, or
delegation logic all fall in this cheap tier.

### Do not use bare /m, it runs the machine out of memory

On 2026-08-01, a DNA-triggered rebuild with bare `/m` (16 parallel nodes on
this machine) failed after 7 minutes 36 seconds with 106 errors, 101 of them
`C1060: compiler is out of heap space`.

The same tree, with the same edits,
rebuilt cleanly at `/m:6` in 8 minutes 49 seconds. The cap costs nothing in
wall-clock time and is simply more reliable.

Use this command instead:

```powershell
& $cmake --build $build --config Release --target INSTALL -- /m:6
```

Recognize this failure, because it looks exactly like a code bug and is not
one. The errors point at innocent bystanders such as `<mutex>`, `<vector>`,
`fmt/format.h`, and `BLI_math_vector_types.hh`.

None of these are the files
you edited, because the DNA cascade puts every node compiling Blender's
heaviest translation units at once. Secondary symptoms in the same log
include:

- `MSB4166: Child node "N" exited prematurely`
- `MSB6006: "Lib.exe" exited with code -1073741502` (`STATUS_DLL_INIT_FAILED`)
- `MSB6003 ... DirectoryNotFoundException` on a `.tlog` path, sometimes
  naming an unrelated project's directory. This is a garbled artifact of the
  dying node, not corruption.

Classify the errors before you investigate anything:

```bash
grep -oE "error C[0-9]{4}" build.log | sort | uniq -c | sort -rn
```

If the result is entirely `C1060`, no compiler ever rejected your code.
Lower `/m` and rebuild. A pagefile peak near 17 GB during the failure is the
corroborating signal.

### Long builds

The first build outlives a typical command timeout. Launch it detached and
poll the log:

```powershell
$log = "$env:TEMP\blender_build.log"
$p = Start-Process $cmake -ArgumentList "--build","`"$build`"","--config","Release","--target","INSTALL","--","/m:6" `
     -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru -WindowStyle Hidden
```

Use `/m:6` rather than bare `/m`, for the reason in the previous section.

## Gotchas

Each of these items cost real time on the first setup.

`make.bat` cannot run unattended. It has two separate blockers.

It prompts interactively for the library download (see [Libraries](#libraries)), and it
invokes `vswhere.exe`, expecting it on PATH.

It falls back to a working
VS2022 detection, but it first prints a confusing message:
`'vswhere.exe' is not recognized`. The direct `cmake --build` path avoids
both problems.

CMake is not on the system PATH. The official instructions say to tick "Add
CMake to the system PATH" in the CMake installer.

There is no standalone
CMake on this machine at all. A bare `cmake` command fails with
`CommandNotFoundException`. Always use the full VS-bundled path, or prepend
its `bin` directory to `$env:PATH` for the session.

The library version tag lags the source version. `lib/windows_x64`
carries tag `v5.2.0` while the source is 5.3.0-alpha. This is normal and is not a
mismatch to fix.

Developers must update library and source versions together. After you pull
upstream changes into `main`, re-run the [Libraries](#libraries) step before
rebuilding, or you may get link errors against stale libraries.

PowerShell versus `cmd.exe`: the official documentation insists on `cmd.exe`
because of `make.bat`. Since this fork does not use `make.bat`, PowerShell is
fine. Note that PowerShell 5.1 has no `&&` operator. Chain commands with `;`
or with `if ($?) { }`.

Do not build from a Git Bash or MSYS shell. MSYS rewrites the `/m` argument
into a Windows path, and the build fails immediately with
`MSBUILD : error MSB1008: Only one project can be specified`. Nothing
compiles. This is a launcher failure, not a code error.

Close Blender before rebuilding. A running `blender.exe` holds a lock on the
binary, so the build compiles everything successfully and then fails at the
very end with:

```
LINK : fatal error LNK1104: cannot open file '...\bin\Release\blender.exe'
```

Close Blender and re-run the build. Only the link and install steps remain,
so it finishes quickly.

Script-only changes still need installing. Blender runs from the scripts in
the build directory, not from the source tree.

After you edit anything under
`scripts/`, either re-run the build (which copies the files) or copy the
files directly into `bin\Release\5.3\scripts\...`. The direct copy is useful
when Blender runs and a full install would fail on the locked
executable.

## Fork workflow

Work happens on `pyareas/addon-space-editor`, branched from `main` at
`027ef661`. Never commit to `main`, so upstream rebases stay clean.

The branch itself is what ships. A second developer adds this repository as
a remote and rebases the commits onto their own base:

```powershell
git remote add pyareas <url>
git fetch pyareas
git rebase --onto <their-base> 027ef661 pyareas/addon-space-editor
```

Build a clean baseline from unmodified `main` before you make changes, so
any later compile failure is unambiguously yours rather than pre-existing.

Switching branches does not require reconfiguring. Reuse the same build
directory and rerun the [Building](#building) step.

See `addon_space_editor_plan.md` in `docs_ui/` for the feature plan.
</content>

