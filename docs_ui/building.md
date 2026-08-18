# Building This Fork (Windows)

Verified 2026-07-31 against `main` @ `027ef661` (Blender 5.3.0 alpha) on Windows 10.

The official docs at <https://developer.blender.org/docs/handbook/building_blender/windows/>
describe a `make.bat` workflow that **does not work unattended on this machine**. See
[§4 Gotchas](#4-gotchas) for why. Use the commands below instead.

---

## 1. Environment

| Component | Path / version on this machine |
| :--- | :--- |
| Source | `<repo>` |
| Build dir | `<build>` |
| Visual Studio | 2022 Enterprise, MSVC 14.36.32532 (`MSVC_VERSION` 1936) |
| CMake | 3.26.0-msvc3, **VS-bundled only — not on system PATH** |
| OptiX SDK | `C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.1.0` — *optional, see §2.4* |
| Libraries | `lib/windows_x64` @ `60d6e96b` (tag `v5.2.0`), 6.5 GB |
| git-lfs | 3.6.1 (required for the libraries) |
| Python | 3.12.0 (required to run `make_update.py` directly) |

CMake lives at:

```
C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

Version requirements are satisfied with room to spare — the tree needs CMake ≥ 3.21
([`CMakeLists.txt:29`](../CMakeLists.txt#L29)) and MSVC ≥ 1928
([`CMakeLists.txt:131`](../CMakeLists.txt#L131)).

---

## 2. One-time setup

### 2.1 Clone

```powershell
git clone --branch main https://projects.blender.org/blender/blender.git <repo>
```

`main` is the alpha branch — do not use an LTS tag. Paths must not contain spaces.

### 2.2 Libraries

**Do not use `make.bat update`** — it prompts `Would you like to download them? (y/n)`
and any non-interactive run answers no, then exits 1. Call the updater directly:

```powershell
cd <repo>
python .\build_files\utils\make_update.py --no-blender
```

`--no-blender` skips the source-repo pull, so it will not try to rebase or fast-forward
your working branch. It still fetches the libraries, which is the only part that matters.
Expect ~6.5 GB and a long silent stretch during `git lfs pull` — that step produces no
incremental output and looks frozen when it is working normally.

Submodule state afterwards should look like this — the three non-Windows entries are
*meant* to stay uninitialized:

```
-ecbd06cf... lib/linux_x64
-a76ef917... lib/macos_arm64
-c65cd3db... lib/windows_arm64
 60d6e96b... lib/windows_x64 (v5.2.0)
```

There are no add-on or test submodules to initialise in 5.3; add-ons live in the
repository itself under `scripts/`.

### 2.3 Configure

Run once. Takes ~20 seconds.

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -S "<repo>" `
         -B "<build>" `
         -G "Visual Studio 17 2022" -A x64 `
         -DOPTIX_ROOT_DIR="C:\ProgramData\NVIDIA Corporation\OptiX SDK 9.1.0"
```

Ending with `-- Build files have been written to: …` means success.

**Do not reconfigure from scratch afterwards.** CMake re-runs itself automatically when
`CMakeLists.txt` files change; a manual wipe costs a full rebuild for nothing. Never
reconfigure while a build is running — changing cache variables invalidates targets
MSBuild is already walking.

### 2.4 OptiX is not needed in this fork

`-DOPTIX_ROOT_DIR=…` is inherited from the previous fork, which modified the Cycles
kernel. **This fork touches only the UI / editor layer, so OptiX is not required.**

Its cost is small either way: the configure summary reports
`WITH_CYCLES_CUDA_BINARIES OFF` and `WITH_CYCLES_HIP_BINARIES OFF`, so no GPU kernels
are compiled — OptiX contributes host-side device code and headers only. Dropping it
saves minutes, not hours.

To build without it, omit `-DOPTIX_ROOT_DIR` and add:

```
-DWITH_CYCLES_DEVICE_OPTIX=OFF
```

If iteration speed matters more than release fidelity, Cycles can be dropped wholesale
with `-DWITH_CYCLES=OFF`, since nothing in this fork's plan touches rendering. Do the
**final, publishable** build with the full default configuration, so the binary matches
what upstream users would compile.

---

## 3. Building

This is the command for both the first build and every rebuild:

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$build = "<build>"
& $cmake --build $build --config Release --target INSTALL -- /m
```

- `--target INSTALL` is **required**. Building the default target leaves DLLs uncopied
  and the resulting `blender.exe` will not launch.
- It is equally required after **Python-only** changes, and that failure is silent
  rather than obvious: `scripts/` is *copied* into
  `bin/Release/<version>/scripts/` at install time, so building only the `blender`
  target leaves Blender running whatever copy of `scripts/startup/bl_ui/*.py` was
  installed last. Edits to `space_addon.py` then have no effect at all, with no error -
  the panel simply never registers. Cost a long debugging detour on 2026-08-18, chasing
  C++ causes for a stale 13-day-old script. If a Python change appears to do nothing,
  check the timestamp of the *installed* copy before anything else.
- `/m` enables parallel MSBuild.

Output binary:

```
<build>\bin\Release\blender.exe
```

### Timings

| Build | Duration |
| :--- | :--- |
Measured 2026-08-01 at `/m:6` on the machine in §1 (16 cores, 32 GB).

| Build | Duration |
| :--- | :--- |
| First full build | 40–90 min |
| Incremental, one editor `.cc` file (`space_addon.cc`) | **1m07s** |
| Incremental, Cycles-only change | ~2 min |
| Incremental, DNA header touched | **8m49s** |

**What actually triggers the DNA cascade: touching the file, not changing the layout.**
The dependency is a timestamp, so a comment-only edit to a `DNA_*.h` costs the same
8-ish minutes as adding a struct member — `makesdna` re-runs and everything downstream
of the generated DNA rebuilds, even when the generated output is byte-identical. Batch
DNA edits together rather than making them one at a time.

By contrast, work confined to `source/blender/editors/space_addon/` (including its own
`addon_intern.hh`, which is *not* a DNA header) recompiles one translation unit and
relinks: about a minute. Runtime-only structs such as `SpaceAddon_Runtime` live there
deliberately — only the opaque pointer to it is in DNA — so changes to the runtime
cache, panel collection, or delegation logic are all in the cheap tier.

### Do not use bare `/m` — it runs the machine out of memory

Observed 2026-08-01: a DNA-triggered rebuild with bare `/m` (16 parallel nodes on this
machine) failed after 7m36s with **106 errors, 101 of them `C1060: compiler is out of
heap space`**. The same tree, same edits, rebuilt cleanly at `/m:6` in 8m49s — so the
cap costs nothing in wall-clock time and is simply more reliable.

Use:

```powershell
& $cmake --build $build --config Release --target INSTALL -- /m:6
```

The failure is worth recognising because **it looks exactly like a code bug and is not
one**. The errors point at innocent bystanders — `<mutex>`, `<vector>`, `fmt/format.h`,
`BLI_math_vector_types.hh` — none of them the files you edited, because the DNA cascade
has every node compiling Blender's heaviest translation units at once. Secondary
symptoms in the same log:

- `MSB4166: Child node "N" exited prematurely`
- `MSB6006: "Lib.exe" exited with code -1073741502` (`STATUS_DLL_INIT_FAILED`)
- `MSB6003 ... DirectoryNotFoundException` on a `.tlog` path, sometimes naming an
  unrelated project's directory — a garbled artefact of the dying node, not corruption

Triage rule: before investigating anything, classify the errors.

```bash
grep -oE "error C[0-9]{4}" build.log | sort | uniq -c | sort -rn
```

If the result is entirely `C1060`, no compiler ever rejected your code — lower `/m` and
rebuild. A pagefile peak near 17 GB during the failure is the corroborating signal.

### Long builds

The first build outlives a typical command timeout. Launch it detached and poll the log:

```powershell
$log = "$env:TEMP\blender_build.log"
$p = Start-Process $cmake -ArgumentList "--build","`"$build`"","--config","Release","--target","INSTALL","--","/m:6" `
     -RedirectStandardOutput $log -RedirectStandardError "$log.err" -PassThru -WindowStyle Hidden
```

Note `/m:6` rather than bare `/m`, for the reason in the previous section.

---

## 4. Gotchas

Each of these cost real time on the first setup.

**`make.bat` cannot run unattended.** Two separate blockers: it prompts interactively for
the library download (§2.2), and it invokes `vswhere.exe` expecting it on PATH — it falls
back to a working VS2022 detection, but prints a confusing
`'vswhere.exe' is not recognized` first. The direct `cmake --build` path avoids both.

**CMake is not on the system PATH.** The official instructions say to tick "Add CMake to
the system PATH" in the CMake installer; there is no standalone CMake on this machine at
all. Bare `cmake` fails with `CommandNotFoundException`. Always use the full VS-bundled
path, or prepend its `bin` directory to `$env:PATH` for the session.

**The library version tag lags the source version.** `lib/windows_x64` is tagged `v5.2.0`
while the source is 5.3.0-alpha. This is normal and not a mismatch to fix.

**Library and source versions must still be updated together.** After pulling upstream
changes into `main`, re-run §2.2 before rebuilding, or you may get link errors against
stale libraries.

**PowerShell vs `cmd.exe`.** The official docs insist on `cmd.exe` because of `make.bat`.
Since this fork does not use `make.bat`, PowerShell is fine — but note PowerShell 5.1 has
no `&&` operator; chain with `;` or `if ($?) { }`.

**Do not build from a Git Bash / MSYS shell.** MSYS rewrites the `/m` argument into a
Windows path, and the build fails immediately with
`MSBUILD : error MSB1008: Only one project can be specified`. Nothing is compiled — this
is a launcher failure, not a code error.

**Close Blender before rebuilding.** A running `blender.exe` holds a lock on the binary,
so the build compiles everything successfully and then fails at the very end with:

```
LINK : fatal error LNK1104: cannot open file '...\bin\Release\blender.exe'
```

Close Blender and re-run; only the link and install steps remain, so it finishes quickly.

**Script-only changes still need installing.** Blender runs from the scripts in the build
directory, not the source tree. After editing anything under `scripts/`, either re-run the
build (which copies them) or copy the files directly into
`bin\Release\5.3\scripts\...`. The direct copy is useful when Blender is running and a
full install would fail on the locked executable.

---

## 5. Fork workflow

Work happens on `pyareas/addon-space-editor`, branched from `main` @ `027ef661`.
`main` is never committed to, so upstream rebases stay clean and the publishable diff is:

```powershell
git format-patch main..pyareas/addon-space-editor
```

Build a clean baseline from unmodified `main` **before** making changes, so any later
compile failure is unambiguously ours rather than pre-existing.

Switching branches does not require reconfiguring — reuse the same build directory and
just rerun §3.

See [addon_space_editor_plan.md](addon_space_editor_plan.md) for the feature plan.
