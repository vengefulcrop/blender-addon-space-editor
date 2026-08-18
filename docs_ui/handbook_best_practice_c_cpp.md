# Best Practice C/C++

# C/C++ Best Practice

Note that this page is in addition to the [style
guide](../c_cpp/) which covers
stylistic aspects of writing code, this page covers other topics
relating to writing code for Blender.

## Order of Operations for "sizeof(..)"

When calculating the size of a value in bytes, order `sizeof` first, eg:
`sizeof(type) * length`, this avoids integer overflow errors by
promoting the second value to `size_t` in the case it's a smaller type
such as an int.

Note that for array allocation, we have `MEM_`**`m`**`alloc_arrayN` and
`MEM_`**`c`**`alloc_arrayN`.

## Comment unused arguments in C++ code

When a C++ function has an unused argument, prefer declaring it like
`int /*my_unused_var*/` to using the `UNUSED()` macro. This is because
the macro does not work properly for MSVC-- it only prevents using the
variable but does not suppress the warning. It also has a more complex
implementation.

## Avoid Unsafe String C-API's

Given the current state of Blender's internal data structures (DNA
structs for example), fixed size char buffers are used making it
necessary to use low level string manipulation.

Unsafe C-API functions should be avoided as these have been a cause of
bugs historically [(see
examples)](https://projects.blender.org/blender/blender/issues/108917).

This table lists function to avoid and alternatives that should be used
instead.

| Unsafe C-API | Safe Alternative |
| --- | --- |
| `strcpy`, `strncpy` | `BLI_strncpy`, or `STRNCPY` macro. |
| `sprintf`, `snprintf` | `BLI_snprintf`, `BLI_snprintf_rlen` or `SNPRINTF`, `SNPRINTF_RLEN` macros. |
| `vsnprintf`, `vsprintf` | `BLI_vsnprintf`, or `VSNPRINTF`, `VSNPRINTF_RLEN` macros. |
| `strcat`, `strncat` | `BLI_strncat`. `BLI_string_join` may also be an alternative for concatenating strings. |

Notes relating to fixed size char buffer use:

- Unless otherwise stated fixed sized char buffers must be null terminated.

- Queries that rely on null termination are acceptable such as `strlen`, `strstr` and related functions.

- When constructing UTF-8 encoded strings which may not fit destination buffer, copy the strings with `BLI_strncpy_utf8`, or use `BLI_str_utf8_invalid_strip` on the resulting string.

- When performing low level operations on byte arrays, it's preferable to calculate sizes and use `memcpy` to construct the buffer.

- Complex logic to construct fixed size char buffers should assert the final size fits within buffer size.

### Exceptions

- Libraries in `extern/` are not maintained as part of Blender's code, this policy doesn't apply there.

- `StringPropertyRNA::get` callbacks (defined in `rna_*.cc`) uses `strcpy` as the string must be large enough to hold a string length defined by `StringPropertyRNA::length`.
