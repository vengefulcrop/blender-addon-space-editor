# pyareas tests

This folder belongs to the pyareas fork. It is not part of upstream Blender.
Every file here tests the Add-on space editor and the screen code this fork
changed. Remove the whole folder before you send a patch to
`projects.blender.org`.

`tests/python/` beside it is upstream. Do not add a fork test there.

## Naming

A file starts with `pyareas_`. Upstream uses the `bl_` prefix, so the two
sets stay apart in a directory listing and in a test report.

## Running a test

A test in this folder drives the user interface, so it needs a running
Blender with a screen. Open the Text Editor, load the file, and press Alt-P.
The Python Console works too:

```python
exec(open("<repo>/tests/pyareas/<file>.py").read())
```

Each test prints its own lines with a bracket prefix. A test that cannot meet
its preconditions says so and stops.

## Files

| File | Covers |
|---|---|
| `pyareas_addon_editor_host.py` | The editor hosts an add-on. It converts the largest free area and points it at one add-on. |
| `pyareas_addon_editor_delegate.py` | Context delegation. A Node Wrangler panel draws through an open Shader Editor. |
| `pyareas_addon_editor_demo.py` | Panel drawing, with a throwaway add-on whose panels have no `poll()`. |
| `pyareas_area_swap_delegate.py` | Defect 14. A swap must clear `ScrArea::context_delegate_spacetype`. |

The first three are manual helpers. They print what they set up, and a person
reads the result on screen. Only `pyareas_area_swap_delegate.py` prints PASS
or FAIL.

The defect list is
[docs_wiki/operations/bugfix.md](../../docs_wiki/operations/bugfix.md). The
concept behind this test is `arch~delegate-field-cleared-on-space-move~1` in
[docs_wiki/architecture/context_delegation.md](../../docs_wiki/architecture/context_delegation.md).
