---
type: reference
title: "C and C++ Coding Style"
description: "Blender's C/C++ naming, formatting, comment, and language conventions not covered by clang-format"
tags: [c, cpp, style, naming, comments, clang-format]
last_updated: 2026-09-12
source: "handbook_c_cpp.md"
verbatim: true
---

<!-- This file is a verbatim copy of an external Blender document.
     Do not rewrite the prose. The ASD-STE100 and register rules in
     docs_wiki/CLAUDE.md do not apply here. -->

# C/C++ Guidelines

# C/C++ Coding Style

While Blender uses auto-formatting
([clang-format](../../tooling/clangformat/)), this page covers aspects
of code style which aren't automated.

There are only two important rules:

- When making changes, conform to the style and conventions of the surrounding code.

- Strive for clarity, even if that means occasionally breaking the guidelines. Use your head and ask for advice if your common sense seems to disagree with the conventions.

## Language/Encoding

There are some over-arching conventions for Blenders code base.

- American English Spelling for all doc-strings variable names and comments.

- Use ASCII where possible, avoid special Unicode characters such as '÷', '' or 'λ'.

- Use UTF-8 encoding for all source files where Unicode characters are required.

- Use Unix-style end of line (`LF`, aka `'\n'` character).

## Naming

- Use descriptive names for global variables and functions.

- Naming should follow the `snake_case` convention.

- Public function names should include the module identifier in all capitals, object and property they're operating and operation itself. Very familiar with RNA callbacks names: `BKE_object_foo_get(...)` / `BKE_object_foo_set(...)`: ``` /* Don't: */ ListBase *curve_editnurbs(Curve *cu); /* Do: */ ListBase *BKE_curve_editnurbs_get(Curve *cu); ```

- Private functions should not start with capitalized module identifier. They can, however, start with lower case module identifier: ``` /* Don't: */ static void DRW_my_utility_function(void); /* Do: */ static void drw_my_utility_function(void); static void my_other_utility_function(void); ```

- Local variables should be short and to the point.

### Size, Length & Count

Variables and struct and class members representing size,
length or count should use the following suffixes:

- `_num`: The number of items in an array, vector or other container.

- `_count`: Accumulated, counted values (such as the number of items in a linked-list).

- `_size`: Size in bytes.

- `_len`: For strings (the length of the string without it's null byte, as used in `strlen`).

For example:

```
/* Struct members. */
struct {
  /* An allocated C array of integers. */
  int *lut;
  /* The number of elements in the `lut` array. */
  int lut_num;
  /* The allocated size of the `lut` pointer, in bytes. */
  size_t lut_size;
}

/* Function arguments. */
void function(int *lut, int lut_num);

```

Use the same suffixes for functions and methods returning that type of data,
with one exception for generic C++ containers: the standard library and our
own `blender::` BLI library use a `size()` method to return their number of items.

For example:

```
int BLI_listbase_count(const ListBase *list);
int BLI_ghash_num(ghash);
int BLI_dynstr_len(ds);

/* But for C++ generic containers: */
/**
 * Return how many values are currently stored in the vector.
 */
int64_t size() const {...}

```

### Constants

- Global constant names should be in all capitals (the `UPPER_CASE` style), whether they are macro `#define` or `constexpr`.

- Class-level constant names should be in all capitals.

### Macros

- All macro names should be in all capitals.

### Enums

- Labels in C-style enums should be in all capitals.

- Labels in C++-style enum classes should be in Pascal case (`EnumType::PascalCase`).

- Enums used in DNA files should have explicit values assigned.

### Function arguments

#### Return arguments

In C its common to use arguments to return values (since C only supports
returning a single value).

- return arguments should have a `r_` prefix, to denote they are return values.

- return arguments should be grouped at the end of the argument list.

- *optionally*, put these arguments on a new line (especially when the argument list is already long and may be split across multiple lines anyway). ``` /* Don't: */ void BKE_curve_function(Curve *cu, int *totvert_orig, int totvert_new, float center[3]); /* Do: */ void BKE_curve_function(Curve *cu, int totvert_new, int *r_totvert_orig, float r_center[3]); ```

Note, some areas in blender use a `_r` as a suffix, eg `center_r`, while
this is **NOT** our convention, we choose not to change all code at this
moment.

### Class data member names

Private/protected data members of a C++ class should have name with a
trailing underscore. Public data members should not have this suffix.

## Value Literals

- `float`/`double` (**f** only for floats): ``` /* Don't: */ float foo = .3; float bar = 1.f; /* Do: */ float foo = 0.3f; float bar = 1.0f; ```

- `bool`: ``` /* Don't: */ bool foo = 1; bool bar = 0; /* Do: */ bool foo = true; bool bar = false; ```

## Integer Types

Note

There is a lot of existing code that does not follow the rules
below yet. Don't do global replacements without talking to a maintainer
beforehand. Also, when interfacing with external libraries, sometimes it
makes sense to follow their policy of integer type usage.

- Only use `int` and `char` of the builtin integer types. Instead of using `short`, `long` or `long long`, use fixed size integer types like `int16_t`. You can assume that `int` has at least 32 bits.

- Use `int64_t` for integers that we know can be “big”.

- Use `bool` with `true` and `false` to represent truth values (instead of int with 0 and 1).

- If your code is a container with a size, be sure its size-type is large enough for any possible usage. When in doubt, use a larger type like `int64_t`.

- Use unsigned integers in bit manipulations and modular arithmetic. When using modular arithmetic, mention that in a comment.

- When using unsigned integers, always use `uint8_t`, `uint16_t`, `uint32_t` or `uint64_t`.

- Don’t use unsigned integers to indicate that a value is non-negative, use assertions instead.

- Since bit operations are used on flags, those should be unsigned integers with a fixed size.

- If your code is using `uint` already, try to avoid doing any arithmetic on values of that type. Additions of small positive constants are likely OK, but avoid subtraction or arithmetic with any values that might be negative.

- When storing a pointer inside an integer cannot be avoided (e.g. to do arithmetic or to sort them), use `intptr_t` and `uintptr_t`.

For code that interfaces external libraries, it may be preferred to use
the types that library uses to avoid unnecessary conversion between
types.

## Operators and Statements

### Switch Statement

There are some conventions to help avoid mistakes.

- blocks of code in a `case` **must** end with a `break` statement, or the macro: `ATTR_FALLTHROUGH;` *Without this its hard to tell when a missing `break` is intentional or not.*

- when a block of code in a `case` statement uses braces, the `break` statement should be within the braces too.

- only use curly braces when introducing `case`-local variables.

```
/* Don't: */
switch (value) {
  case TEST_A: {
    int a = func();
    result = a + 10;
  } break;        // NO: break outside braces.
  case TEST_B:
    func_b();
  case TEST_C:
  case TEST_D: {  // NO: unnecessary braces.
    func_c();
  } break;        // NO: break outside braces.
}

/* Do: */
switch (value) {
  case TEST_A: {
    int a = func();
    result = a + 10;
    break;
  }
  case TEST_B:
    func_b();
    ATTR_FALLTHROUGH;
  case TEST_C:
  case TEST_D:
    func_c();
    break;
}

```

## Braces

### Always Use Braces

Braces are to be used even when not strictly necessary ([omission can
lead to
errors](https://softwareengineering.stackexchange.com/a/320264/99957)).

```
/* Don't: */
if (a == b)
  d = 1;
else
  c = 2;

/* Do: */
if (a == b) {
  d = 1;
}
else {
  c = 2;
}

```

```
/* Don't: */
for (int i = 0; i

## Indentation

In C/C++ sources use 2 spaces for indentation.

## Trailing Space

All files have trailing white-space stripped, if you can - configure
your editor to strip trailing space on save.

## Comments

- Write in the third person perspective, to the point, using the same terminology as the code *(think good quality technical documentation).*

- Be sure to explain non-obvious algorithms, hidden assumptions, implicit dependencies, and design decisions and the reasons behind them.

- Acronyms should always be written in upper-case (write `API` not `api`).

- Use proper sentences with capitalized words and a full-stop. ``` /* My small comment. */ ``` NOT ``` /* my small comment */ ```

**Tags**

Tags should be formatted as follows:

```
/* TODO: body text. */

```

Or optionally, some information can be included:

- **Unique user name from `projects.blender.org`** ``` /* TODO(@username): body text. */ ```

- **linking to the task associated with the `TODO`** ``` /* TODO(#123): body text. */ ```

- **linking to the pull request associated with the `TODO`.** ``` /* TODO(#123): body text. */ ```

**Common Tags**

- `NOTE`

- `TODO`

- `FIXME`

- `WORKAROUND` use instead of `HACK`.

- `XXX` general alert, prefer one of the more descriptive tags (above) where possible. This should be limited to describing usage of a non-obvious solution caused by some design limitations which better be resolved after rethinking of design. Comments should describe the problem and how it may be fixed, not only flagging the issue.

**[Literal Strings](https://www.doxygen.nl/manual/markdown.html#md_codespan) (following markdown)**

Code or any text that isn't plain English should be surrounded by
back-ticks, e.g:

```
/* This comment includes the expression `x->y / 2` using back-ticks. */

```

**[Symbols](https://www.doxygen.nl/manual/autolink.html#linkother) (following doxygen)**

References to symbols such as a function, structs, enum values... etc
should start with a `#`. e.g:

```
/** Remove by #wmGroupType.type_update_flag. */

```

**Email Addresses**
Email formatting should use angle brackets, matching git `Full Name `.

### C/C++ Comments

C-style comments should be used in C++ code.

Adding dead code is discouraged. In some cases, however, having unused
code is useful (gives more semantic meaning, provides reference
implementation, ...).

It is fine having unused code in this cases. Use `//` for a
single-line code, and `#if 0` for multi-line code. And always explain
what the unused code is about.

- When using multi-line comments, markers (star character, `*`) should be used in the beginning of every line of comment: ``` /* Special case: ima always local immediately. Clone image should only * have one user anyway. */ ``` NOT ``` /* Special case: ima always local immediately. Clone image should only have one user anyway. */ ```

### Comment Sections

It's common to use comments to group related code in a file. Blender's
convention is to use doxygen formatted sections.

```
/* -------------------------------------------------------------------- */
/** \name Title of Code Section
 * \{ */

... code ...

/** \} */

```

You may include descriptive text about the section under the title:

```
/* -------------------------------------------------------------------- */
/** \name Title of Code Section
 *
 * Explain in more detail the purpose of the section.
 * \{ */

... code ...

/** \} */

```

For headers that mainly contain declarations, the following non-doxy
sections are also acceptable:

```
/* --------------------------------------------------------------------
 * Name of the section.
 */

```

Or with some extra text:

```
/* --------------------------------------------------------------------
 * Name of the section.
 *
 * Optional description.
 */

```

### API Docs

When writing more comprehensive comments that include for example,
function arguments and return values, cross references to other
functions... etc, we use [Doxygen](http://doxygen.org) syntax comments.

If you choose to write doxygen comments, here's an example of a typical
doxy comment (many more in blenders code).

```
/**
 * Return the unicode length of a string.
 *
 * \param start: the string to measure the length.
 * \param maxlen: the string length (in bytes)
 * \return the unicode length (not in bytes!)
 */
size_t BLI_strnlen_utf8(const char *start, const size_t maxlen);

```

Note that this is just the typical paragraph style used in blender with
an extra leading `'*'`.

As for placement of documentation, follow these guidelines:

- **Symbols (functions, constants, structs, classes, etc.) that are declared in a header file** are considered part of the module's *public interface*, and should be documented in the header file. This makes it possible to document & organize the header file in a way that makes sense to the reader, to document groups of symbols together, and to read through the available functionality without being hindered by implementation details and internal code. This documentation should describe the public interface, but not internal implementation details that are irrelevant to calling code.

- Symbols that are **internal to a file** (static, anonymous namespace) should be documented at the implementation. This allows forward-declaring such functions in the implementation file, then listing the higher-level public functions, and only then have the lower-level internal/helper functions with their documentation. The documentation can be more to the point when the higher-level concepts are already known to the reader (when reading top-to-bottom through he file).

- **Implementation details** that are irrelevant to the calling code should be documented at the definition/implementation of the symbol. Sometimes such information can even go inside a function, when it applies only to a part of its internals.

When there is overlap between internal and public functions, for example
when two public functions actually call an internal function with some
additional parameters, the internal function's documentation can refer
to the public function. That way documentation doesn't have to be copied
between those.

In Summary:

- Try to make it possible for developers to use a module by only reading its header file. In other words, improve [black-boxing](https://en.wikipedia.org/wiki/Black_box) by documenting the public symbols in the header file.

- Optionally use doxygen comments for detailed docs.

- Keep comments about implementation details close to the implementation.

- Try to avoid duplication of comments between header & implementation doc-strings. From an internal symbol, just refer to the public one instead of copying its comments.

- These guidelines also apply to `*_internal.h` headers.

- When a symbol has two blocks of documentation (for example public doc in the header file, and implementation details doc in the `.c` file), only use formal parameter and return documentation (`\param` and `\return`) in the public doc-string. Doxygen cannot deal with having those defined twice in different files.

## Clang Format

Blender uses [Clang format](../../tooling/clangformat/) which is the
required way to ensure styling for C, C++ & GLSL code.

### Turning Clang Format Off

In some cases clang-format doesn't format code well or produces
significantly less readable output.

You may disable clang-format in this case with:

```
/* clang-format off */

... manually formatted code ...

/* clang-format on */

```

Note that this should be isolated to the region of code where it's
needed.

## Utility Macros

Typically we try to avoid wrapping functionality into macros, but there
are some limited cases where its useful to have standard macros, which
can be shared across the code-base.

Currently these are stored in
[BLI_utildefines.h](https://projects.blender.org/blender/blender/src/branch/main/source/blender/blenlib/BLI_utildefines.h$1).

A brief list of common macros we suggest to use:

- **`SWAP(type, a, b)`**: Swap 2 values. In C++ code, prefer **`std::swap`**

- **`ELEM(value, other, vars...) ...`**: Check if the first argument matches one of the following values given.

- **`POINTER_AS_INT(value), POINTER_FROM_INT`**: warning free int/pointer conversions (for use when it wont break 64bit).

- **`STRINGIFY(id)`**: Represent an identifier as a string using the preprocessor.

- **`STREQ(a, b), STRCASEEQ(a, b)`**: String comparison to avoid confusion with different uses of `strcmp()`.

- **`STREQLEN(a, b, len), STRCASEEQLEN(a, b, len)`**: Same as STREQ but pass a length value.

Other utility macros:

- **`AT`**: Convenience for `__file__:__line__`. Example use: **`printf("Current location " AT " of the file\n");`**

- **`BLI_assert(test)`**: Assertion that prints by default (only aborts when `WITH_ASSERT_ABORT` is defined).

- **`BLI_assert_unreachable()`**: Assertion for code that should never be reached in a valid execution.

- **`BLI_INLINE`**: Portable prefix for inline functions.

Many lesser used macros are defined in **`BLI_utildefines.h`**, but the
main ones are covered above.

## UI Messages

**Common rules**

- “Channel” identifiers, like X, Y, Z, R, G, B, etc. are always capitalized!

- Do not use abbreviations like “verts” or “VGroups”, always use plain words like “vertices” or “vertex groups”.

- Do not use English contractions like “aren’t”, “can’t”, etc. Better to keep full spelling, “are not” or “cannot” are not that much longer, and it helps keeping consistency styling over the whole UI.

- Some data names are supposed to be “title cased” (namely datablocks), even in tips. However, it is a very fuzzy rule (e.g. vertex groups are not datablocks…), so better never use such emphasis if you are unsure.

**UI labels**

- They must use English “title case”, i.e. each word is capitalized (Like In This Example).

**UI tooltips**

- They are built as usual sentences. However: They should use infinitive as much as possible: "Make the character run", **not** "Makes the character run".

- They must not end with a point. This also implies they should be made of a single sentence (“middle” points are *ugly*!), so use comas and parenthesis: "A mesh-like surface encompassing (i.e. shrinkwrap over) all vertices (best results with fewer vertices)", **not** "A mesh-like surface encompassing (i.e. shrinkwrap over) all vertices. Best results with fewer vertices."

## File Size

If possible try keep files under roughly 4000 lines of code. While there
will be exceptions to this rule, you might consider if files over this
size can be logically split up.

*This is more a rule of thumb, not a hard limit.*

## Filename Extensions

- C files should be named `.c` and `.h`.

- C++ files should be named `.cc` and `.hh`, although `.cpp`, `.hpp` and `.h` are sometimes used as well. As a rule of thumb, keep files in a single module consistent but use the preferred naming in new code.

## C++ Namespaces

Namespaces have lower case names.

Blender uses the top-level `blender` namespace. Most code should be in
nested namespaces like `blender::deg` or `blender::io::alembic`. The
exception are common data structures in the `blenlib` folder, that can
exist in the blender namespace directly (e.g. `blender::float3`).

Prefer using nested namespace definition like
`namespace blender::io::alembic { ... }` over
`namespace blender { namespace io { namespace alembic { ... }}}`.

Tests should be in the same namespace as the code they are testing.

### Anonymous Namespace

The `static` keyword is preferred over the anonymous namespace for
file-private functions, as this makes it possible to locally see the
scoping rule of that function without having to scroll to a potentially
far away location to find the enclosing namespace declaration. Note that
this is not a hard rule, but rather a preference.

The anonymous namespace can be used for making variables and class
declarations file-private.

### Unity builder namespace

Files which a part of a [unity build](../../tooling/unity_builds/)
should have their private-to-compile-unit symbols inside a
`blender::::unity_build__cc`:

```
namespace blender::deg {

namespace unity_build_deg_node_cc {

/* Function which is only used within the node.cc file */
static void some_private_function() { ... }

}  // namespace unity_build_deg_node_cc

/* Function which is declared in a public header (is a part of public API). */
void function_which_is_public_in_the_module() { ... }

}  // namespace blender::deg

```

This ensures that concatenation of files for unity builder does not
cause symbol conflicts, while keeping it clear for the developers the
intent of the namespace which is unique to the translation unit.

## C++ Containers

Prefer using our own containers over their corresponding alternatives in
the standard library. Common containers in the `blender::` namespace are
`Vector`, `Array`, `Set` and `Map`.

Prefer using `blender::Span` or `blender::MutableSpan` (passed by
value rather than by reference) as function parameters over
`const blender::Vector``&` or `const blender::Array``&`.

## String Formatting

Use the [fmt](https://fmt.dev/) library (`#include `) for formatting strings, instead of e.g. `std::format`.

## No C++ Modules

Don't use C++20 modules but stick to normal header files. A much larger effort is necessary to properly investigate module support for Blender.

## No C++ Coroutines

Don't use C++20 coroutines. There are no clear use-cases currently that justify adding the complexity. If use-cases become apparent, the usage of coroutines can be investigated more thoroughly.

## C++ Type Cast

For
[arithmetic](https://en.cppreference.com/w/c/language/arithmetic_types)
and [enumeration](https://en.cppreference.com/w/c/language/enum) types
use the [functional-style cast
(2)](https://en.cppreference.com/w/cpp/language/explicit_cast).

```
int my_int = int(float_value);
float my_float = float(int_value);

```

Follow this decision tree when down-casting polymorphic types:

```
flowchart
  can_avoid_downcast["Is design without down-casting appropriate? E.g. using virtual methods."]
  use_no_cast["Don't use explicit casting."]
  is_type_check_necessary["Is a type check necessary?"]
  use_dynamic_cast_ptr["Use dynamic_cast with a pointer type. Always check the returned pointer."]
  is_performance_sensitive["Is performance sensitive?"]
  use_static_cast["Use static_cast for best performance. Hard to find bug if assumption is wrong."]
  use_dynamic_cast_ref["Use dynamic_cast with a reference type. Throws an exception if type is wrong."]

  can_avoid_downcast --"yes"--> use_no_cast
  can_avoid_downcast --"no"--> is_type_check_necessary
  is_type_check_necessary --"yes"--> use_dynamic_cast_ptr
  is_type_check_necessary --"no"--> is_performance_sensitive
  is_performance_sensitive --"yes"--> use_static_cast
  is_performance_sensitive --"no"--> use_dynamic_cast_ref
```

For other type conversions use `static_cast` when possible and
`reinterpret_cast` or `const_cast` otherwise.

```
void *user_data;

MyCallbackData *data = static_cast(user_data);
SubsurfModifierData *smd = reinterpret_cast(md);

```

## Variable Scope

Try to keep the scope of variables as small as possible.

```
/* Don't: */
int a, b;

a = ...;
...
b = ...;

/* Do: */
int a = ...;
...
int b = ...;

```

## Const

Use `const` whenever possible. Try to write your code so that `const`
can be used, i.e. prefer declaring new variables instead of mutating
existing ones.

Certain `const` declarations in function parameters are irrelevant to
the declaration and only necessary in the function definition:

```
/* No const necessary in declaration because `param` is passed by value. */
void func(float param);

/* In the definition, it means that `param` will not change value. */
void func(const float param) { ... }

```

## Implicit & Deducted Typing

### Some General Rules

Do not consider contextual help from IDEs as a good reason to remove
explicitness from the source code. While they can be very handy, not all
IDEs provide the same level of contextual information, and the code must
remain understandable when such help is not available (e.g. when
reviewing PRs online).

### auto

In general, `auto` should not be used unless the type is clearly and
unambiguously expressed somewhere else in the same expression, e.g.
by using a casting expression.

```
/* Do: */
/* The type of `var` is clear from the casting of the assigned data. */
auto *var = static_cast *>(user_data);
const bool result = my_callback(my_id);

/* Don't: */
/* There is no immediate way to know the type of `result`. */
auto result = my_callback(my_id);

```

Note that `auto` should also be used with unnamed types, e.g. to store a
lambda (closure type) in a local variable, when there is no other choice
to store a local callback (i.e. when a `blender::FunctionRef` or similar
is not used).

Another valid usage of `auto` is with iterators: Usually, the exact type
of the iterator does not matter, as long as they are just used through
the common iterator patterns.

```
blender::Array my_array;

/* ... */

/* Do: */
for (auto my_it = my_array.rbegin(); my_it != my_array.rend(); my_it++) {
  int my_val = *my_it;
  /* ... */
}

/* Don't: */
for (auto my_it = my_array.rbegin(); my_it != my_array.rend(); my_it++) {
  auto my_val = *my_it;
  /* ... */
}

/* Do: */
for (int my_val : my_array) {
  /* ... */
}

/* Don't: */
for (auto my_val : my_array) {
  /* ... */
}

```

There is an exception for the `enumerate()` method to iterate with an index, as there is no C++ syntax to specify the item type.

```
for (const auto [index, item] : my_list.enumerate()) {
  /* ... */
}

```

### Template Type Deduction

Do not rely on type deduction when using templated functions or types,
unless being explicit adds no value in term of readability and safety
of the code.

Here are some heuristics to help decide when to be explicit or not:
* Prefer explicit typing when:
  - The types can be expressed clearly and with concision.
  - Being explicit clarifies the expected behavior of the templated
    expression.
* Prefer type deduction when:
  - The types are verbose to express.
  - The types are not defined locally (e.g. defined by another embedding
    template).
  - The types are easy to infer from the code, without a detailed analysis
    of the whole expression.
* Templated function calls should usually have explicit template parameters,
  when these affect their return types. This is consistent with the fact that
  there is no type inference on the returned value in C++.
* Templated types (classes etc.) should have explicit template parameters,
  unless they have a default value defined.

Example of type deduction that can always be implicit:

```
/* Even though this would seem fairly obvious, the created data type has to be
 * explicitly defined (no type inference on usages of the returned value). */
ID *id = MEM_new();

// ...

/* There is no need to call explicitly `MEM_delete(id)` here. */
MEM_delete(id);

```

In some cases, being explicit does not help with readability, e.g.
when nesting templates, calling templated functions inside of
templated functions, ...). In such cases, it can be best to fully
rely on type deduction.

```
template
inline void AngleAxisRotatePoint(const T angle_axis[3],
                                 const T pt[3],
                                 T result[3]);

/* ... */

template
bool my_func()(const T* const intrinsics,
                const T* const R_t,
                const T* const X,
                T* residuals) const {
  T x[3];
  /* `my_func` only use one templated type, explicitely calling
   * `ceres::AngleAxisRotatePoint(R_t, X, x)` here would not add any useful
   * information. */
  ceres::AngleAxisRotatePoint(R_t, X, x);
  /* ... */
  return true;
}

```

Example where being explicit about the type of processed data improves local readability,
and reduces chances of potential hidden bugs (due e.g. to implicit conversion between
different types of numeric values):
`threading::parallel_reduce`.

```
int val = 0;
blender::Vector my_vec;

// ...

/* Don't: */
/* Nothing in the call below would clearly indicates the type of computed value.
 * Further more, a mistake in one of the parameter types or callbacks definition
 * can lead to many lines of fairly obscure compiler errors.
 * It can also make logic errors due to implicit conversion harder to spot,
 * e.g. passing a float `0.0f` value instead of an integer one
 * to the `identity` parameter.
 */
val = threading::parallel_reduce(
    my_vec,
    1024,
    0,
    [](const IndexRange range, int value) { return value + int(range[0]); },
    [](const int &a, const int &b) { return a + b; });

/* Do: */
/* Here it is obvious what the _intended_ produced value type is.
 * It will also often generate much more concise and readable compiler error
 * messages in case of mistakes.
 * Note that while the `Value` template parameter is specified, both callbacks ones
 * (`Function` and `Reduction`) are left to type deduction, as specifying them
 * explicitely would add a lot of verbosity, and their type information is already
 * clear from the lambdas definitions.
 */
val = threading::parallel_reduce(
    my_vec,
    1024,
    0,
    [](const IndexRange range, int value) -> int { return value + int(range[0]); },
    [](const int &a, const int &b) -> int { return a + b; });

```

## Class Layout

Classes should be structured as follows. Parts that are not needed by a
specific class should just be skipped.

```
class X {
  /* using declarations */
  /* static data members */
  /* non-static data members */

 public:
  /* default constructor */
  /* other constructors */
  /* copy constructor */
  /* move constructor */

  /* destructor */

  /* copy assignment operator */
  /* move assignment operator */
  /* other operator overloads */

  /* all public static methods */
  /* all public non-static methods */

 protected:
  /* all protected static methods */
  /* all protected non-static methods */

 private:
  /* all private static methods */
  /* all private non-static methods */
};

```

## Using this->

Use `this->` when accessing methods and data members that don't have a
[trailing underscore](#class-data-member-names).

```
class X {
 private:
  float my_float_;

 public:
  int my_int;

  void foo() {
    /* Use `this->` because there is no trailing underscore. */
    this->my_int = 42;
    this->bar();

    /* Do *not* use `this->` because there is a trailing underscore. */
    my_float_ = 3.14f;
  }

  void bar() {
    ...
  }
};

```

## Tests

[Unit tests](../../testing/setup/) can be created in Python (in
`tests/python`) or in C++. This section describes the latter.

Each module can generate its own test library. The tests in these
libraries are then bundled into a single executable. This executable can
be run with `ctest`; even though the tests reside in a single
executable, they are still exposed as individual tests to ctest, and
thus can be selected via its `-R` argument.

The following rules apply:

- Tests that target functionality in `somefile.{c,cc}` should reside in `somefile_test.cc` in the same directory. For an example, see [armature_test.cc](https://projects.blender.org/blender/blender/src/branch/main/source/blender/blenkernel/intern/armature_test.cc).

- Tests that target other functionality, for example in a public header file, should be placed in `source/blender/{modulename}/tests`. For an example, see [io/usd/tests](https://projects.blender.org/blender/blender/src/branch/main/source/blender/io/usd/tests/).

- The namespace for tests is the `tests` sub-namespace of the code under test. For example, tests for `blender::bke` should be in `blender::bke:tests`. Note that for test selection purposes, the name of each test should still be unique, regardless of the namespace it is in.

- The test files should be listed in the module's `CMakeLists.txt` in a `blender_add_test_lib()` call. See [the blenkernel module](https://projects.blender.org/blender/blender/src/branch/main/source/blender/blenkernel/CMakeLists.txt) for an example.

# Related Topics

- See: [Blender Tools](../../tooling/blender_tools/) (includes style checker)

- See: Presentation [Crockford on JavaScript - Section 8: Programming Style & Your Brain](https://www.youtube.com/watch?v=taaEzHI9xyY_Crockford_on_JavaScript_-_Section_8:_Programming_Style_&_Your_Brain) --- applies to C/C++ too.
