---
type: reference
title: "Blend File Compatibility"
description: "Backward and forward compatibility expectations for .blend files, and how to handle breakages in development projects"
tags: [blend-file, compatibility, versioning, dna]
last_updated: 2026-09-12
source: "handbook_compatibility_handling_for_blend_files.md"
verbatim: true
---

<!-- This file is a verbatim copy of an external Blender document.
     Do not rewrite the prose. The ASD-STE100 and register rules in
     docs_wiki/CLAUDE.md do not apply here. -->

# Compatibility Handling for .blend Files

# Blend File Compatibility

This document addresses expectations regarding compatibility of .blend
files across blender versions. It also gives guidelines for how to
handle development projects involving major breakages in compatibility.

To summarize, general expectations are:

- Backward compatibility: Blender is expected to be able to open files saved with any previous versions, although some major changes conversion code may be removed after that the related feature has been deprecated for at least two years.

- Forward compatibility: Blender is expected to be able to open files saved with relatively more recent versions, though with some loss of data.

- Critical forward compatibility breakages are only allowed every two years, when the major release cycle number is increased (e.g. from 3.x to 4.0). These are changes in data or features that cause either massive loss of data, or complete inability to open files in older Blender versions.

- The latest LTS of the previous release cycle is expect to open, and act as a converter, between newer and older file versions.

## Definitions

There are two types of compatibility topics, depending on whether the
considered executable opens a .blend file saved with an older version of
the software, or a newer one.

- [Backward compatibility](https://en.wikipedia.org/wiki/Backward_compatibility) is the ability of a software to open files saved with older versions of itself.

- [Forward compatibility](https://en.wikipedia.org/wiki/Forward_compatibility) is the ability of a software to open files saved with newer versions of itself.

Breakage of compatibility is when a software cannot fully load or
properly use the data in the file it opens, leading to a wide range of
issues (loss of data, incorrect results, crashes...). These breakages
can be regrouped within a few categories, with various consequences:

- The loaded data is completely unknown from current version of the software. It is ignored/invisible in current version, and lost if the file is saved from that version.

- It is ignored/invisible in current version, but re-written as-is if the file is saved from that version.

- The loaded data is known from the current version, but has a different meaning (is interpreted differently). The result once open is incorrect, but the data structure remains valid. Saving the file from that version may propagate/amplify the problems.

- Opening the file results in a severely broken data, with potentially crashes on load and/or editing, and/or file corruption if saving the file from that version.

## Blender Handling of Compatibility

The lower-level data model of .blend files is designed to support both
backward and forward compatibility. This is achieved through several
aspects:

- .blend file store the version of Blender used to generate them, and the minimum Blender version expected to be able to open them.

- The DNA (the data model) is written in the .blend files.

- When reading a .blend file: Unknown data is ignored.

- Missing data is initialized with default values.

- A versioning code is executed, which incrementally applies all required conversion processes from the initial version of the .blend file to the current version of the software.

### Backward Compatibility

Blender strives to guarantee a complete backward compatibility, with some
very rare exceptions.

Any crash or file corruption caused by loading a file saved with an
older version of Blender is considered as a severe or critical bug.

Loss of data is extremely rare, widely discussed and accepted by the
development team, and extremely well documented and publicized in
advance. In practice, even when removing or replacing a significant
feature, best efforts are made to convert the old data to an 'as best as
possible' version of it in the newer data model.

This is achieved by:

- Keeping the old data model in the code base (tagged as deprecated).

- Adding conversion code in the various 'do_version' code paths, and ensuring the .blend file version is bumped when needed.

In most cases, keeping the versioning code and the deprecated data in
the code base is not a significant problem, and there is no hard 'end of
life' for such versioning. This is why Blender 4.0 can still open .blend
files from over 20 years ago and keep a reasonable amount of data.

In some cases however, it is not reasonable to clutter the code base
indefinitely with large amount of deprecated data structures and code.
In such case, after some time, newer versions of Blender won't be able
to convert the old deprecated data anymore, and this one will be lost on
file loading. Users are then expected to use an older, intermediary
version of the software to perform the conversion, when needed. Examples
include the old, pre-2.5 animation system.

In general, any file saved within a given `n` major version of Blender
are expected to open without any significant loss of data in any later
versions of Blender in the major `n` and `n+1` range. E.g. any
Blender 4.x version is expected to open any file saved with Blender 3.x
and get a complete, fully usable and editable data.

### Forward Compatibility

Breakages in forward compatibility context are unavoidable. Blender
strives to ensure that no critical file corruption or crashes happen in
such case, and that the user is properly warned when an action is
expected to lead to loss of data (e.g. by opening and saving a newer
.blend file with an older version of Blender).

In forward compatibility context, the cases defined above can be
considered that way:

- Non-critical breakages: Unknown data (i.e. new feature or data type): This is the common, expected breaking case. An older version of Blender is not expected to understand nor use data or feature that did not exist back then. It typically also cannot re-save it.

- In some rare cases there is support to (partially) save back unknown data (e.g. data defined from python scripts and stored as IDProperties). This should not be expected or relied on though.

- Critical breakages: Modification of existing feature or data type, leading to major loss of data when opened with older Blender versions, or even crashes or data corruption. By replacing a deprecated data by a new one. E.g. recently the replacement of Proxies by LibOverrides, or the future replacement of Greasepencil v2 by the v3.

- Refactor 'in place', where the internal organization and meaning of the data changes, but the general container / DNA definition (often an ID) remains partially the same. E.g. recently, the refactor of the Mesh data-block. *(Note that this case is the most likely to cause crashes or data corruption when opening with an older Blender version)*.

While data loss is expected and unavoidable for new features (the
non-critical breakages), Blender will always ensure that there is no
critical compatibility breakages in existing features within a major
release cycle.

Entering a new major release cycle is the moment where critical
compatibility breakages are allowed. However, best efforts are made to
keep the latest LTS release of the previous major cycle compatible with
these changes. It is then intended that this last LTS of major version
`n-1` can be used as conversion tool for files generated by versions
`n`, to make them usable by any Blender from versions `n-1`.

E.g. Blender 3.6 LTS is able to open the new Mesh data from Blender 4.0
onward, and to save it back into a format compatible with all Blender
3.x versions.

## Recommendations for Handling Compatibility in Development Projects

Almost any project affecting existing feature or data has to take
compatibility into account. In most cases, ensuring backward
compatibility is relatively trivial (adding a few lines of conversion
code in `do_version` code-base), and forward compatibility breakages
are of non-critical types, so do not require any particular handling. A
good practice nonetheless is to try opening the new version of a Blend
file in at least the two actively maintained LTS releases, to ensure no
critical forward compatibility breakages sneaked in.

More ambitious projects, especially these deeply modifying existing data
models or features, are very likely to induce critical compatibility
breakages. Often these can be avoided, or at least mitigated, by a
design carefully taking into account the existing code-base. But
sometimes there is just no way to avoid them. How to handle
compatibility (in both directions) should be an integral part of the
design of such projects:

- A dedicated section of the design task (or even a dedicated sub-task when things get very big) should cover compatibility handling.

- This compatibility section should always be reviewed by Core developers, and any critical breakages should be accepted by a majority of the Core team.

- The task should be tagged with the `Interest/Compatibility` label.

- It is the responsibility of the design author(s) to ensure that there is awareness and acceptance of their expected critical breakages among the rest of the developers team.

- Regarding timing, critical breakages can only be committed in main branch during the first few months of a new major release cycle, roughly every two years.

- Extensive and repeated testing should be performed during the development of the project to ensure compatibility is behaving as expected.
