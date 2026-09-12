---
type: reference
title: "C and C++ Coding Style"
description: "Blender's C/C++ naming, formatting, comment, and language conventions not covered by clang-format"
tags: [c, cpp, style, naming, comments, clang-format]
last_updated: 2026-09-12
---

# C and C++ Coding Style

Blender uses auto-formatting with clang-format. This page covers aspects of
code style that clang-format does not automate.

There are only two important rules.

- When you make changes, conform to the style and conventions of the
  surrounding code.
- Strive for clarity, even if that means you occasionally break a
  guideline. Use your judgment, and ask for advice when your judgment
  disagrees with a convention.

## Language and encoding

These conventions apply across Blender's code base.

- Use American English spelling for all doc-strings, variable names, and
  comments.
- Use ASCII where possible. Avoid special Unicode characters such as
  '÷', '' or 'λ'.
- Use UTF-8 encoding for source files that require Unicode characters.
- Use Unix-style end of line (`LF`, the `'\n'` character).

## Naming

- Use descriptive names for global variables and functions.
- Follow the `snake_case` convention for names.
- Public function names must include the module identifier in all
  capitals, the object and property they operate on, and the operation
  itself. This matches the RNA callback naming pattern, for example
  `BKE_object_foo_get(...)` and `BKE_object_foo_set(...)`.

  ```
  /* Don't: */
  ListBase *curve_editnurbs(Curve *cu);
  /* Do: */
  ListBase *BKE_curve_editnurbs_get(Curve *cu);
  ```

- Private functions must not start with a capitalized module identifier.
  A private function can start with a lower case module identifier.

  ```
  /* Don't: */
  static void DRW_my_utility_function(void);
  /* Do: */
  static void drw_my_utility_function(void);
  static void my_other_utility_function(void);
  ```

- Local variables must have short, to the point names.

### Size, length, and count

Use these suffixes for variables and struct or class members that
represent a size, length, or count.

- `_num`: the number of items in an array, vector, or other container.
- `_count`: an accumulated, counted value, such as the number of items in a
  linked list.
- `_size`: a size in bytes.
- `_len`: the length of a string, without its null byte, matching the
  convention of `strlen`.

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

Use the same suffixes for functions and methods that return that type of
data. There is one exception for generic C++ containers: the standard
library and Blender's own `blender::` BLI library use a `size()` method to
return their number of items.

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

- Global constant names must be in all capitals (`UPPER_CASE`), whether
  they are a macro `#define` or a `constexpr`.
- Class-level constant names must be in all capitals.

### Macros

- All macro names must be in all capitals.

### Enums

- Labels in C-style enums must be in all capitals.
- Labels in C++-style enum classes must be in Pascal case
  (`EnumType::PascalCase`).
- Enums used in DNA files must have explicit values assigned.

### Function arguments

#### Return arguments

C commonly uses arguments to return values, because C supports only a
single return value.

- Return arguments must have an `r_` prefix, to denote that they are
  return values.
- Group return arguments at the end of the argument list.
- You may optionally put these arguments on a new line, especially when
  the argument list is already long and may be split across multiple
  lines anyway.

  ```
  /* Don't: */
  void BKE_curve_function(Curve *cu, int *totvert_orig, int totvert_new, float center[3]);
  /* Do: */
  void BKE_curve_function(Curve *cu, int totvert_new, int *r_totvert_orig, float r_center[3]);
  ```

Some areas in Blender use `_r` as a suffix, for example `center_r`. This is
not the convention. The team has chosen not to change all existing code to
match at this time.

### Class data member names

Give private or protected data members of a C++ class a name with a
trailing underscore. Public data members must not have this suffix.

## Value literals

- Use a trailing `f` only for `float` values, not for `double` values.

  ```
  /* Don't: */
  float foo = .3;
  float bar = 1.f;
  /* Do: */
  float foo = 0.3f;
  float bar = 1.0f;
  ```

- Use `true` and `false` for `bool` values.

  ```
  /* Don't: */
  bool foo = 1;
  bool bar = 0;
  /* Do: */
  bool foo = true;
  bool bar = false;
  ```

## Integer types

There is a lot of existing code that does not follow the rules below yet.
Do not make global replacements without talking to a maintainer
beforehand. Also, when you interface with an external library, it
sometimes makes sense to follow that library's policy on integer type
usage.

- Use only `int` and `char` from the built-in integer types. Instead of
  `short`, `long`, or `long long`, use a fixed size integer type such as
  `int16_t`. You can assume that `int` has at least 32 bits.
- Use `int64_t` for integers that can be "big".
- Use `bool` with `true` and `false` to represent truth values, instead of
  `int` with `0` and `1`.
- If your code is a container with a size, make sure its size type is
  large enough for any possible usage. When in doubt, use a larger type
  such as `int64_t`.
- Use unsigned integers in bit manipulations and modular arithmetic. When
  you use modular arithmetic, mention that fact in a comment.
- When you use unsigned integers, always use `uint8_t`, `uint16_t`,
  `uint32_t`, or `uint64_t`.
- Do not use unsigned integers to indicate that a value is non-negative.
  Use assertions instead.
- Since bit operations act on flags, flags must be unsigned integers with
  a fixed size.
- If your code already uses `uint`, avoid arithmetic on values of that
  type where possible. Additions of small positive constants are likely
  fine, but avoid subtraction or arithmetic with any value that might be
  negative.
- When you cannot avoid storing a pointer inside an integer, for example
  to do arithmetic on it or to sort it, use `intptr_t` and `uintptr_t`.

For code that interfaces with external libraries, it may be preferable to
use the types that library uses, to avoid unnecessary conversion between
types.

## Operators and statements

### Switch statement

Follow these conventions to help avoid mistakes.

- A block of code in a `case` must end with a `break` statement, or with
  the macro `ATTR_FALLTHROUGH;`. Without this, it is hard to tell whether
  a missing `break` is intentional.
- When a block of code in a `case` statement uses braces, put the `break`
  statement inside the braces too.
- Use curly braces only when you introduce `case`-local variables.

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

### Always use braces

Use braces even when they are not strictly necessary. Omitting braces can
lead to errors (see this
[discussion](https://softwareengineering.stackexchange.com/a/320264/99957)).

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

The source material's original example continues with a truncated `for`
loop fragment, preserved here as it exists in the source:

```
/* Don't: */
for (int i = 0; i
```

## Indentation

Use 2 spaces for indentation in C and C++ sources.

## Trailing space

Strip trailing white-space from all files. Configure your editor to strip
trailing space on save, if it supports that option.

## Comments

- Write comments in the third person perspective, to the point, using the
  same terminology as the code. Aim for good quality technical
  documentation.
- Explain non-obvious algorithms, hidden assumptions, implicit
  dependencies, design decisions, and the reasons behind them.
- Write acronyms in upper case (write `API`, not `api`).
- Use proper sentences with capitalized words and a full stop.

  ```
  /* My small comment. */
  ```

  Not:

  ```
  /* my small comment */
  ```

### Tags

Format tags as follows.

```
/* TODO: body text. */
```

You may optionally include additional information.

- A unique user name from `projects.blender.org`:

  ```
  /* TODO(@username): body text. */
  ```

- A link to the task associated with the TODO:

  ```
  /* TODO(#123): body text. */
  ```

- A link to the pull request associated with the TODO:

  ```
  /* TODO(#123): body text. */
  ```

Common tags are:

- `NOTE`
- `TODO`
- `FIXME`
- `WORKAROUND`, use this instead of `HACK`.
- `XXX`, a general alert. Prefer one of the more descriptive tags above
  where possible. Limit `XXX` to describing the use of a non-obvious
  solution caused by a design limitation, one that is better resolved
  after the design is rethought. The comment must describe the problem and
  how to fix it, not only flag the issue.

### Literal strings

Following [markdown conventions](https://www.doxygen.nl/manual/markdown.html#md_codespan),
surround code or any text that is not plain English with back-ticks. For
example:

```
/* This comment includes the expression `x->y / 2` using back-ticks. */
```

### Symbols

Following [doxygen conventions](https://www.doxygen.nl/manual/autolink.html#linkother),
start a reference to a symbol, such as a function, struct, or enum value,
with a `#`. For example:

```
/** Remove by #wmGroupType.type_update_flag. */
```

### Email addresses

Format an email address with angle brackets, matching the git format
`Full Name <email>`.

### C and C++ comments

Use C-style comments in C++ code.

Adding dead code is discouraged. In some cases, however, unused code is
useful. It gives more semantic meaning, or it provides a reference
implementation.

It is fine to have unused code in these cases. Use `//` for a single line
of code, and `#if 0` for multiple lines. Always explain what the unused
code is about.

- When you use a multi-line comment, put a marker, the star character
  `*`, at the beginning of every comment line:

  ```
  /* Special case: ima always local immediately. Clone image should only
   * have one user anyway. */
  ```

  Not:

  ```
  /* Special case: ima always local immediately. Clone image should only have one user anyway. */
  ```

### Comment sections

Use comments to group related code in a file. Blender's convention uses
doxygen-formatted sections.

```
/* -------------------------------------------------------------------- */
/** \name Title of Code Section
 * \{ */

... code ...

/** \} */
```

You may include descriptive text about the section under the title.

```
/* -------------------------------------------------------------------- */
/** \name Title of Code Section
 *
 * Explain in more detail the purpose of the section.
 * \{ */

... code ...

/** \} */
```

For headers that mainly contain declarations, the following non-doxygen
section format is also acceptable.

```
/* --------------------------------------------------------------------
 * Name of the section.
 */
```

Or with extra text:

```
/* --------------------------------------------------------------------
 * Name of the section.
 *
 * Optional description.
 */
```

### API docs

When you write a more comprehensive comment that includes, for example,
function arguments, return values, or cross references to other
functions, use [Doxygen](http://doxygen.org) syntax comments.

Here is an example of a typical doxygen comment, in the style used
throughout Blender's code.

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

This paragraph style matches Blender's typical style, with an extra
leading `*`.

Follow these guidelines for the placement of documentation.

- Document symbols (functions, constants, structs, classes, and so on)
  that are declared in a header file, in the header file. This is because
  a header symbol is part of the module's public interface. Documenting it
  in the header lets you organize the header in a way that makes sense to
  the reader, document groups of symbols together, and read through the
  available functionality without implementation details getting in the
  way. This documentation must describe the public interface, not internal
  implementation details that are irrelevant to calling code.
- Document symbols that are internal to a file (static, or in an anonymous
  namespace) at the implementation. This lets you forward-declare such
  functions in the implementation file, list the higher-level public
  functions first, and only then list the lower-level internal or helper
  functions with their documentation. The documentation can be more direct
  once the higher-level concepts are already known to the reader, when
  they read top to bottom through the file.
- Document implementation details that are irrelevant to calling code at
  the definition or implementation of the symbol. Sometimes such
  information belongs inside a function, when it applies only to part of
  its internals.

When there is overlap between an internal and a public function, for
example when two public functions call an internal function with some
extra parameters, the internal function's documentation can refer to the
public function. This way the documentation does not need to be copied
between the two.

In summary:

- Make it possible for developers to use a module by reading only its
  header file. In other words, improve
  [black-boxing](https://en.wikipedia.org/wiki/Black_box) by documenting
  the public symbols in the header file.
- Optionally use doxygen comments for detailed documentation.
- Keep comments about implementation details close to the implementation.
- Avoid duplicating comments between the header and the implementation
  doc-strings. From an internal symbol, refer to the public one instead of
  copying its comments.
- These guidelines also apply to `*_internal.h` headers.
- When a symbol has two blocks of documentation, for example public
  documentation in the header file and implementation detail
  documentation in the `.c` file, use formal parameter and return
  documentation (`\param` and `\return`) only in the public doc-string.
  Doxygen cannot handle those tags defined twice in different files.

## Clang format

Blender uses clang-format, which is the required way to ensure consistent
styling for C, C++, and GLSL code.

### Turning clang-format off

In some cases, clang-format does not format code well, or it produces
significantly less readable output.

You may disable clang-format in this case with:

```
/* clang-format off */

... manually formatted code ...

/* clang-format on */
```

Isolate this disabling to the region of code where you need it.

## Utility macros

Blender typically avoids wrapping functionality into macros, but there are
some limited cases where a standard macro is useful, shared across the
whole code base.

Currently these macros are stored in
[BLI_utildefines.h](https://projects.blender.org/blender/blender/src/branch/main/source/blender/blenlib/BLI_utildefines.h).

Here is a brief list of common macros to use.

- `SWAP(type, a, b)`: swap two values. In C++ code, prefer `std::swap`.
- `ELEM(value, other, vars...)`: check whether the first argument matches
  one of the given values.
- `POINTER_AS_INT(value)`, `POINTER_FROM_INT`: warning-free int and pointer
  conversions, for use when the conversion will not break on 64-bit
  systems.
- `STRINGIFY(id)`: represent an identifier as a string, using the
  preprocessor.
- `STREQ(a, b)`, `STRCASEEQ(a, b)`: string comparison macros, to avoid
  confusion between different uses of `strcmp()`.
- `STREQLEN(a, b, len)`, `STRCASEEQLEN(a, b, len)`: the same as `STREQ`,
  but with a length value.

Other utility macros:

- `AT`: a convenience for `__file__:__line__`. Example use:
  `printf("Current location " AT " of the file\n");`.
- `BLI_assert(test)`: an assertion that prints by default. It aborts only
  when `WITH_ASSERT_ABORT` is defined.
- `BLI_assert_unreachable()`: an assertion for code that must never run in
  a valid execution.
- `BLI_INLINE`: a portable prefix for inline functions.

`BLI_utildefines.h` defines many lesser used macros, but the list above
covers the main ones.

## UI messages

### Common rules

- Always capitalize channel identifiers, such as X, Y, Z, R, G, and B.
- Do not use abbreviations such as "verts" or "VGroups". Always use plain
  words such as "vertices" or "vertex groups".
- Do not use English contractions such as "aren't" or "can't". Keep the
  full spelling, "are not" or "cannot". These forms are not much longer,
  and they keep the UI style consistent.
- Some data names, namely datablocks, must be title cased, even in tips.
  This rule is fuzzy, since for example vertex groups are not datablocks.
  Do not use this emphasis when you are unsure.

### UI labels

- Use English title case, where each word is capitalized (Like In This
  Example).

### UI tooltips

- Build tooltips as normal sentences. Use the infinitive form as much as
  possible: write "Make the character run", not "Makes the character
  run".
- Do not end a tooltip with a full stop. This implies the tooltip must be
  a single sentence, since a "middle" full stop looks bad. Use commas and
  parentheses instead. Write "A mesh-like surface encompassing (i.e.
  shrinkwrap over) all vertices (best results with fewer vertices)", not
  "A mesh-like surface encompassing (i.e. shrinkwrap over) all vertices.
  Best results with fewer vertices."

## File size

Where possible, keep files under roughly 4000 lines of code. There will be
exceptions to this rule. Consider whether a file over this size can be
logically split up.

This is a rule of thumb, not a hard limit.

## Filename extensions

- Name C files `.c` and `.h`.
- Name C++ files `.cc` and `.hh`, although `.cpp`, `.hpp`, and `.h` are
  sometimes used as well. As a rule of thumb, keep files in a single
  module consistent, but use the preferred naming for new code.

## C++ namespaces

Give namespaces lower case names.

Blender uses the top-level `blender` namespace. Put most code in a nested
namespace such as `blender::deg` or `blender::io::alembic`. The exception
is common data structures in the `blenlib` folder, which can exist
directly in the `blender` namespace, for example `blender::float3`.

Prefer a nested namespace definition, such as
`namespace blender::io::alembic { ... }`, over
`namespace blender { namespace io { namespace alembic { ... }}}`.

Put tests in the same namespace as the code they test.

### Anonymous namespace

Prefer the `static` keyword over the anonymous namespace for file-private
functions. This lets a reader locally see the scoping rule of a function
without scrolling to a potentially far away enclosing namespace
declaration. This is not a hard rule, but a preference.

You can use the anonymous namespace to make variables and class
declarations file-private.

### Unity builder namespace

Put private-to-compile-unit symbols of files that are part of a unity
build inside a `blender::::unity_build__cc` namespace.

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

This ensures that the unity builder's concatenation of files does not
cause symbol conflicts, while it keeps clear to developers the intent of
the namespace that is unique to the translation unit.

## C++ containers

Prefer Blender's own containers over their corresponding standard library
alternatives. Common containers in the `blender::` namespace are `Vector`,
`Array`, `Set`, and `Map`.

Prefer `blender::Span` or `blender::MutableSpan`, passed by value rather
than by reference, as function parameters, over `const blender::Vector&`
or `const blender::Array&`.

## String formatting

Use the [fmt](https://fmt.dev/) library for formatting strings, instead of
for example `std::format`.

## No C++ modules

Do not use C++20 modules. Stick to normal header files. Proper
investigation of module support for Blender needs a much larger effort.

## No C++ coroutines

Do not use C++20 coroutines. There are no clear use cases currently that
justify the added complexity. If use cases become apparent, the team can
investigate coroutine usage more thoroughly.

## C++ type cast

For [arithmetic](https://en.cppreference.com/w/c/language/arithmetic_types)
and [enumeration](https://en.cppreference.com/w/c/language/enum) types, use
the [functional-style cast](https://en.cppreference.com/w/cpp/language/explicit_cast).

```
int my_int = int(float_value);
float my_float = float(int_value);
```

Follow this decision tree when you down-cast polymorphic types.

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

For other type conversions, use `static_cast` when possible, and
`reinterpret_cast` or `const_cast` otherwise.

```
void *user_data;

MyCallbackData *data = static_cast(user_data);
SubsurfModifierData *smd = reinterpret_cast(md);
```

## Variable scope

Keep the scope of a variable as small as possible.

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

Use `const` whenever possible. Write your code so that you can use
`const`. Prefer declaring a new variable over mutating an existing one.

Certain `const` declarations in function parameters are irrelevant to the
declaration and are necessary only in the function definition.

```
/* No const necessary in declaration because `param` is passed by value. */
void func(float param);

/* In the definition, it means that `param` will not change value. */
void func(const float param) { ... }
```

## Implicit and deduced typing

### Some general rules

Do not treat contextual help from an IDE as a good reason to remove
explicitness from the source code. IDE help can be handy, but not every
IDE provides the same level of contextual information, and the code must
stay understandable when such help is not available, for example when
reviewing a pull request online.

### auto

In general, do not use `auto` unless the type is clearly and unambiguously
expressed somewhere else in the same expression, for example through a
casting expression.

```
/* Do: */
/* The type of `var` is clear from the casting of the assigned data. */
auto *var = static_cast *>(user_data);
const bool result = my_callback(my_id);

/* Don't: */
/* There is no immediate way to know the type of `result`. */
auto result = my_callback(my_id);
```

Also use `auto` with unnamed types, for example to store a lambda (a
closure type) in a local variable, when there is no other way to store a
local callback (that is, when a `blender::FunctionRef` or similar type is
not an option).

Another valid use of `auto` is with iterators. Usually, the exact type of
the iterator does not matter, as long as the code uses only common
iterator patterns.

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

There is an exception for the `enumerate()` method used to iterate with an
index, since there is no C++ syntax to specify the item type.

```
for (const auto [index, item] : my_list.enumerate()) {
  /* ... */
}
```

### Template type deduction

Do not rely on type deduction when you use templated functions or types,
unless being explicit adds no value in terms of readability and safety of
the code.

Use these heuristics to decide when to be explicit.

Prefer explicit typing when:

- You can express the types clearly and concisely.
- Being explicit clarifies the expected behavior of the templated
  expression.

Prefer type deduction when:

- The types are verbose to express.
- The types are not defined locally, for example when another embedding
  template defines them.
- The types are easy to infer from the code, without a detailed analysis
  of the whole expression.

Templated function calls must usually have explicit template parameters
when these parameters affect the return type. This is consistent with the
fact that C++ has no type inference on the returned value.

Templated types, such as classes, must have explicit template parameters,
unless they have a default value defined.

This example shows type deduction that can always be implicit.

```
/* Even though this would seem fairly obvious, the created data type has to be
 * explicitly defined (no type inference on usages of the returned value). */
ID *id = MEM_new();

// ...

/* There is no need to call explicitly `MEM_delete(id)` here. */
MEM_delete(id);
```

In some cases, being explicit does not help readability, for example when
nesting templates or calling templated functions inside templated
functions. In such cases, it can be best to rely fully on type deduction.

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

This example shows a case where being explicit about the type of processed
data improves local readability, and reduces the chance of a hidden bug,
for example due to an implicit conversion between different types of
numeric values, in `threading::parallel_reduce`.

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

## Class layout

Structure a class as follows. Skip any part that a specific class does
not need.

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

Use `this->` when you access a method or data member that has no
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

You can create unit tests in Python, in `tests/python`, or in C++. This
section describes the C++ tests.

Each module generates its own test library. The tests in these libraries
are then bundled into a single executable. Run this executable with
`ctest`. Even though the tests reside in a single executable, ctest still
exposes them as individual tests, so you can select them with the `-R`
argument.

Follow these rules.

- Put tests that target functionality in `somefile.{c,cc}` in
  `somefile_test.cc`, in the same directory. For an example, see
  [armature_test.cc](https://projects.blender.org/blender/blender/src/branch/main/source/blender/blenkernel/intern/armature_test.cc).
- Put tests that target other functionality, for example in a public
  header file, in `source/blender/{modulename}/tests`. For an example, see
  [io/usd/tests](https://projects.blender.org/blender/blender/src/branch/main/source/blender/io/usd/tests/).
- Put tests in the `tests` sub-namespace of the code under test. For
  example, put tests for `blender::bke` in `blender::bke::tests`. For test
  selection purposes, the name of each test must still be unique,
  regardless of the namespace it is in.
- List the test files in the module's `CMakeLists.txt`, in a
  `blender_add_test_lib()` call. See
  [the blenkernel module](https://projects.blender.org/blender/blender/src/branch/main/source/blender/blenkernel/CMakeLists.txt)
  for an example.

## Related topics

- See Blender Tools, which includes the style checker, referenced from the
  original handbook at `../../tooling/blender_tools/`.
- See the presentation
  [Crockford on JavaScript, Section 8: Programming Style and Your Brain](https://www.youtube.com/watch?v=taaEzHI9xyY_Crockford_on_JavaScript_-_Section_8:_Programming_Style_&_Your_Brain).
  Its content applies to C and C++ too.
</content>
