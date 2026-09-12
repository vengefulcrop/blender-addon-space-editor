---
type: reference
title: "Blend File Compatibility"
description: "Backward and forward compatibility expectations for .blend files, and how to handle breakages in development projects"
tags: [blend-file, compatibility, versioning, dna]
last_updated: 2026-09-12
---

# Blend File Compatibility

This document addresses expectations about compatibility of `.blend` files
across Blender versions. It also gives guidelines for how to handle
development projects that involve major breakages in compatibility.

In summary, the general expectations are:

- Backward compatibility: Blender is expected to open files saved with any
  previous version, although a major change may remove the conversion code
  after the related feature has been deprecated for at least two years.
- Forward compatibility: Blender is expected to open files saved with
  relatively more recent versions, though with some loss of data.
- Critical forward compatibility breakages are allowed only every two
  years, when the major release cycle number increases (for example from
  3.x to 4.0). These are changes in data or features that cause either
  massive loss of data, or complete inability to open files in older
  Blender versions.
- The latest LTS release of the previous release cycle is expected to open,
  and act as a converter, between newer and older file versions.

## Definitions

There are two types of compatibility topics. They depend on whether the
executable opens a `.blend` file saved with an older version of the
software, or with a newer one.

- [Backward compatibility](https://en.wikipedia.org/wiki/Backward_compatibility)
  is the ability of software to open files saved with older versions of
  itself.
- [Forward compatibility](https://en.wikipedia.org/wiki/Forward_compatibility)
  is the ability of software to open files saved with newer versions of
  itself.

A breakage of compatibility happens when software cannot fully load or
properly use the data in the file it opens. This leads to a range of
issues: loss of data, incorrect results, or crashes. Breakages fall into a
few categories, with different consequences.

- The loaded data is completely unknown to the current version of the
  software. It is ignored or invisible in the current version, and it is
  lost if you save the file from that version.
- The data is ignored or invisible in the current version, but the software
  rewrites it as-is if you save the file from that version.
- The current version knows the loaded data, but interprets it with a
  different meaning. The result once open is incorrect, but the data
  structure remains valid. Saving the file from that version may propagate
  or amplify the problems.
- Opening the file results in severely broken data, with potential crashes
  on load or edit, or file corruption if you save the file from that
  version.

## Blender handling of compatibility

The lower-level data model of `.blend` files is designed to support both
backward and forward compatibility. Blender achieves this through several
mechanisms.

- A `.blend` file stores the version of Blender used to generate it, and
  the minimum Blender version expected to open it.
- The DNA, the data model, is written into the `.blend` file.
- When Blender reads a `.blend` file, it ignores unknown data.
- Blender initializes missing data with default values.
- Versioning code runs and incrementally applies all required conversion
  processes, from the initial version of the `.blend` file to the current
  version of the software.

### Backward compatibility

Blender strives to guarantee complete backward compatibility, with very
rare exceptions.

Any crash or file corruption caused by loading a file saved with an older
version of Blender counts as a severe or critical bug.

Loss of data is extremely rare. The development team widely discusses and
accepts each case, and documents and publicizes it well in advance. In
practice, even when a significant feature is removed or replaced, the team
makes a best effort to convert the old data into an as-good-as-possible
version of it in the newer data model.

Blender achieves this through two practices.

- It keeps the old data model in the code base, tagged as deprecated.
- It adds conversion code in the various `do_version` code paths, and it
  ensures the `.blend` file version is bumped when needed.

In most cases, keeping the versioning code and the deprecated data in the
code base is not a significant problem, and there is no hard end of life
for such versioning. This is why Blender 4.0 can still open `.blend` files
from over 20 years ago and keep a reasonable amount of data.

In some cases, it is not reasonable to clutter the code base indefinitely
with a large amount of deprecated data structures and code. In such a case,
after some time, newer versions of Blender stop converting the old
deprecated data, and this data is lost on file loading. Users are then
expected to use an older, intermediary version of the software to perform
the conversion, when needed. One example is the old, pre-2.5 animation
system.

In general, any file saved within a given major version `n` of Blender is
expected to open without significant loss of data in any later version of
Blender in the major `n` and `n+1` range. For example, any Blender 4.x
version is expected to open any file saved with Blender 3.x and produce
complete, fully usable, and editable data.

### Forward compatibility

Breakages in a forward compatibility context are unavoidable. Blender
strives to ensure that no critical file corruption or crash happens in such
a case, and that the user gets a proper warning when an action is expected
to cause loss of data (for example when opening and saving a newer
`.blend` file with an older version of Blender).

In a forward compatibility context, the cases defined above apply as
follows.

- Non-critical breakages: unknown data, that is, a new feature or data
  type. This is the common, expected breaking case. An older version of
  Blender is not expected to understand or use data or a feature that did
  not exist at that time. It typically also cannot save it back.
- In some rare cases, Blender partially supports saving back unknown data
  (for example data defined from Python scripts and stored as
  IDProperties). Do not expect or rely on this support.
- Critical breakages: modification of an existing feature or data type,
  leading to major loss of data when opened with older Blender versions, or
  even to crashes or data corruption. This happens by replacing deprecated
  data with new data. For example, the recent replacement of Proxies by
  LibOverrides, or the future replacement of Grease Pencil v2 by v3.
- Refactor in place, where the internal organization and meaning of the
  data changes, but the general container or DNA definition (often an ID)
  remains partially the same. For example, the recent refactor of the Mesh
  data-block. Note that this case is the most likely to cause crashes or
  data corruption when you open the file with an older Blender version.

While data loss is expected and unavoidable for new features, the
non-critical breakages, Blender always ensures there are no critical
compatibility breakages in existing features within a major release cycle.

Entering a new major release cycle is the moment when critical
compatibility breakages are allowed. The team makes a best effort to keep
the latest LTS release of the previous major cycle compatible with these
changes. The intent is that this last LTS of major version `n-1` can serve
as a conversion tool for files generated by version `n`, to make them
usable by any Blender from version `n-1`.

For example, Blender 3.6 LTS can open the new Mesh data from Blender 4.0
onward, and it can save that data back into a format compatible with all
Blender 3.x versions.

## Recommendations for handling compatibility in development projects

Almost any project that affects an existing feature or data must take
compatibility into account. In most cases, backward compatibility is
relatively trivial to ensure. You add a few lines of conversion code in the
`do_version` code base, and forward compatibility breakages are of the
non-critical type, so they need no particular handling. A good practice
nonetheless is to try opening the new version of a blend file in at least
the two actively maintained LTS releases, to ensure no critical forward
compatibility breakages sneaked in.

More ambitious projects, especially ones that deeply modify existing data
models or features, are very likely to induce critical compatibility
breakages. A design that carefully accounts for the existing code base can
often avoid or mitigate these breakages. But sometimes there is no way to
avoid them. How you handle compatibility, in both directions, must be an
integral part of the design of such projects.

- A dedicated section of the design task, or even a dedicated sub-task when
  things get very big, must cover compatibility handling.
- Core developers must always review this compatibility section, and a
  majority of the Core team must accept any critical breakages.
- Tag the task with the `Interest/Compatibility` label.
- The design authors are responsible for ensuring that the rest of the
  development team is aware of and accepts the expected critical
  breakages.
- On timing, the team can commit critical breakages to the main branch only
  during the first few months of a new major release cycle, roughly every
  two years.
- Perform extensive and repeated testing during the development of the
  project, to ensure compatibility behaves as expected.
</content>
