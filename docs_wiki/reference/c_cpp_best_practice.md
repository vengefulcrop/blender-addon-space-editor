---
type: reference
title: "C/C++ Best Practice"
description: "Practical rules for Blender C/C++ code beyond style: sizeof order, unused arguments, and safe string APIs"
tags: [c, cpp, best-practice, strings]
last_updated: 2026-09-12
---

# C/C++ Best Practice

This page is in addition to the [C and C++ style guide](./c_cpp_style.md),
which covers stylistic aspects of writing code. This page covers other
topics related to writing code for Blender.

## Order of operations for sizeof(..)

When you calculate the size of a value in bytes, place `sizeof` first. For
example, write `sizeof(type) * length`. This order avoids integer overflow
errors, because it promotes the second value to `size_t` when that value is
a smaller type such as `int`.

For array allocation, use `MEM_mallocN` or `MEM_callocN` (the array
variants: `MEM_mallocN` becomes `MEM_mallocarrayN` and `MEM_callocN`
becomes `MEM_callocarrayN`).

## Comment unused arguments in C++ code

When a C++ function has an unused argument, declare it like
`int /*my_unused_var*/`. Do not use the `UNUSED()` macro. The macro does not
work correctly for MSVC. It only prevents the compiler from using the
variable, but it does not suppress the warning. It also has a more complex
implementation.

## Avoid unsafe string C-APIs

Blender's internal data structures, for example DNA structs, use fixed size
char buffers. This makes low level string manipulation necessary.

Avoid unsafe C-API functions. These functions have caused bugs historically.
See the [example issue](https://projects.blender.org/blender/blender/issues/108917).

This table lists functions to avoid and the alternatives to use instead.

| Unsafe C-API | Safe alternative |
| --- | --- |
| `strcpy`, `strncpy` | `BLI_strncpy`, or the `STRNCPY` macro. |
| `sprintf`, `snprintf` | `BLI_snprintf`, `BLI_snprintf_rlen`, or the `SNPRINTF`, `SNPRINTF_RLEN` macros. |
| `vsnprintf`, `vsprintf` | `BLI_vsnprintf`, or the `VSNPRINTF`, `VSNPRINTF_RLEN` macros. |
| `strcat`, `strncat` | `BLI_strncat`. `BLI_string_join` may also work as an alternative for concatenating strings. |

Follow these notes about fixed size char buffer use.

- Unless stated otherwise, terminate fixed sized char buffers with a null
  byte.
- Queries that rely on null termination are acceptable, such as `strlen`,
  `strstr`, and related functions.
- When you construct UTF-8 encoded strings that may not fit the destination
  buffer, copy the strings with `BLI_strncpy_utf8`, or use
  `BLI_str_utf8_invalid_strip` on the resulting string.
- When you perform low level operations on byte arrays, calculate sizes and
  use `memcpy` to construct the buffer.
- When you construct fixed size char buffers with complex logic, assert
  that the final size fits within the buffer size.

### Exceptions

- Libraries in `extern/` are not maintained as part of Blender's code base.
  This policy does not apply there.
- The `StringPropertyRNA::get` callbacks, defined in `rna_*.cc`, use
  `strcpy`, because the string must be large enough to hold a string length
  defined by `StringPropertyRNA::length`.
</content>
