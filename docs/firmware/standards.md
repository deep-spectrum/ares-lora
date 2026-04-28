# Coding Standards

!!! abstract

    This is a short document describing the coding standards of the Ares-LoRa
    Firmware.

## 1) Language

The programming language allowed for the firmware is C. C++ and Rust are not allowed.
The language and standard used is C23.

## 2) Organization

Project organization is very important for quickly navigating the
project so it is paramount that things are placed in the correct
locations.

### 2.1) Headers

All headers must be placed in the `include` directory. If a group of header files are related to each other, then they can
be grouped in a subdirectory in `include`.

### 2.2) Sources

All sources must be placed in the `src` directory. If a group of source files are related to each other, they can be
grouped in a subdirectory in `src`.

### 2.3) Overlay Files

Device overlay files must be put into the `overlay` directory. Overlay files for specific boards must be named as such:
`<board name>_<mcu>.overlay`. Generic overlay files used by multiple must have a name that describes the purpose of that
overlay file.

### 2.4) Kconfig files

Kconfig files must be placed in the `config` directory except for the main Kconfig file. The suffix of the Kconfig file
must describe the module/modules the configuration declarations are targeting.

## 3) Code Style

This project has a coding style. When writing your code, you don\'t have
to worry too much about code formatting as there is a tool that will do
that for you automatically. However, there are a few things that we ask
for you to keep in mind that the formatting tool will not catch.

### 3.1) Braces for single-line control statements

In C, the curly braces around single-line control
statements can be omitted. However, in this repository, it is considered
bad practice to not wrap the single-line control statements because it
can lead to confusion in the codebase and introduce subtle bugs that are
difficult to spot. All control statements --- `if`, `else`, `for`,
`while`, `do-while`, `switch`, and `case` --- must always have curly
braces, even if they only contain a single statement.

``` {.C}
// Not OK
if (x < 0)
    x = 0;

// OK
if (x < 0) {
    x = 0;
}

// Not OK
switch(foo) {
case 0: doSomething();
    break;
case 1: doOtherThing();
    break;
default: doError();
    break;
}

// OK
switch(foo) {
case 0: {
    doSomething();
    break;
}
case 1: {
    doOtherThing();
    break;
}
default: {
    doError();
    break;
}
}
```

### 3.2) Control Flow

Control flow is an integral part of programs, however, if used
improperly, it will make the code look like a giant heap of dog shit.
There are a few common practices to consider when using control flow.

#### 3.2.1) Nesting

Nesting control flow is OK for some things, however, excessive nesting
becomes a problem. Only 1 level of nesting is allowed. If you need 2 or
more levels of nesting, maybe consider converting your flow chart into a
state machine or breaking things up into multiple functions.
Additionally, C implements [short-circuit
evaluation](https://www.geeksforgeeks.org/c/short-circuit-evaluation-in-programming/),
so that should be used to reduce nesting.

#### 3.2.2) Cyclomatic Redundancy

There is a metric in software that measures the complexity called
\"Strict Cyclomatic Complexity.\" This not only measures the amount of
paths (via nesting) your software can take, it also takes the amount of
conditions that need to be taken into consideration for all the flow
paths. Consider the two function implementations below:

``` {.C}
void fun1(bool a, bool b) {
    if (a) {
        if (b) {
            ...
        }
    }
}

void fun2(bool a, bool b) {
    if (a && b) {
        ...
    }
}
```

Say that `fun1` and `fun2` do the exact same thing. Under normal McCabe
Cyclomatic Complexity, `fun2` (MCC of 2) would be considered better
because it has 1 less branch than `fun1` (MCC of 3). However, under
Strict Cyclomatic Complexity, they would be equivalent because each
logical operation adds a branch.

To maintain simplicity and maintainability of the code, it is asked that
most functions have an SCC of 10 or below and no functions exceed an SCC
of 15.

#### 3.2.3) Centralized exiting of functions

Albeit deprecated by some people, the equivalent of the goto statement
is used frequently by compilers in form of the unconditional jump
instruction.

The goto statement comes in handy when a function exits from multiple
locations and some common work such as cleanup has to be done. If there
is no cleanup needed then just return directly.

Choose label names which say what the goto does or why the goto exists.
An example of a good name could be `out_free_buffer:` if the goto frees
`buffer`. Avoid using GW-BASIC names like `err1:` and `err2:`, as you
would have to renumber them if you ever add or remove exit paths, and
they make correctness difficult to verify anyway.

The rationale for using gotos is:

-   unconditional statements are easier to understand and follow
-   nesting is reduced
-   errors by not updating individual exit points when making
    modifications are prevented
-   saves the compiler work to optimize redundant code away ;)

``` {.C}
int fun(int a) {
    int result = 0;
    char *buffer;

    buffer = malloc(SIZE);
    if (!buffer) {
        return -ENOMEM;
    }

    ...

    if (condition1) {
        while (loop1) {
            ...
        }
        result = 1;
        goto out_free_buffer;
    }
    ...
out_free_buffer:
    free(buffer);
    return result;
}
```

A common type of bug to be aware of is `one err bugs` which look like
this:

``` {.C}
err:
    free(foo->bar);
    free(foo);
    return ret;
```

The bug in this code is that on some exit paths foo is NULL. Normally
the fix for this is to split it up into two error labels `err_free_bar:`
and `err_free_foo:`:

``` {.C}
err_free_bar:
    free(foo->bar);
err_free_foo:
    free(foo);
    return ret;
```

Ideally you should simulate errors to test all exit paths.

### 3.3) Error Codes

If something fails in a function, or if the input parameters are invalid,
then it is important to indicate that somehow. To indicate that, the public
facing APIs should return a standard negative error code whenever applicable. The error codes
are declared in [errno.h](https://docs.zephyrproject.org/latest/doxygen/html/group__system__errno.html).

### 3.4) Commenting

Comments are good, but there is a danger to over commenting. Do not
explain how your code works in a comment. It is much better to write the
code so that the **working** is obvious, and it\'s a waste of time to
explain badly written code.

Generally, you want your comments to tell WHAT your code does, not HOW.
Also, try to avoid putting comments inside a function body: if the
function is so complex that you need to separately comment parts of it,
you should probably take a look at functions. You can make small
comments to note or warn about something particularly clever (or ugly),
but try to avoid excess. Instead, put the comments at the head of the
function, telling people what it does, and possibly WHY it does it.

When documenting an API, it is only necessary to document the public API (header
files). There is no need to document the private API.

When documenting functions, the input parameters as well as the return
value should be documented. Each data structure should be documented. Public
facing comments should be styled after [Doxygen Comment
Blocks](https://www.doxygen.nl/manual/docblocks.html#specialblock), and
private comments should be kept as normal comments (so they don\'t get
included in the doc generation).

## 4) Submitting a PR

You wrote an extension, and now you want it to be part of the next
release. Awesome! There are a few things we ask of you before submitting
a PR.

### 4.1) Check the SCC of your code

Run the following command:

``` {.bash .copy}
make scc
```

It should spit out a report of the C++ code base. Fix anything that
reports more than 15, and consider simplifying the functions that report
between 11 and 15.

### 4.2) Check the code style

Run the following command:

``` {.bash .copy}
make style
```

Consider fixing all the warnings that get spit out, however, any checks
that got disabled the files and the line numbers need to be noted in the
PR.

### 4.3) Format the code

Consistent formatting is necessary to maintain readability of the code
base. Luckily, there\'s a tool that does it for you already. Just run
the following command:

``` {.bash .copy}
make format
```

Just make sure to commit your code after running that command.

### 4.4) Ready to submit the PR now?

After running the checks and the formatter, you should be ready to
submit your PR now.