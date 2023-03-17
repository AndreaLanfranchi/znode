# Moria - C++ Zen Node Implementation

C++ Implementation of ZEN node based on Thorax Architecture

## Table of Contents

- [About Moria](#about-moria)
- [Obtaining source code](#obtaining-source-code)
- [Building on Linux & MacOS](#building-on-linux--macos)
- [Building on Windows](#building-on-windows)
- [Tree Map](#tree-map)
- [Style Guide](#style-guide)
- Go to [Documentation](./doc)

[CMake]: http://cmake.org
[Google's C++ Style Guide]: https://google.github.io/styleguide/cppguide.html
[libmdbx]: https://gitflic.ru/project/erthink/
[Visual Studio]: https://www.visualstudio.com/downloads
[VSCode]: https://www.visualstudio.com/downloads
[CLion]: https://www.jetbrains.com/clion/download/
[submodules]: https://git-scm.com/book/en/v2/Git-Tools-Submoduleshttps://git-scm.com/book/en/v2/Git-Tools-Submodules

## About Moria

Moria is a semi-greenfield C++ implementation of the ZEN protocol.  
It aims to be the fastest Zen node implementation while maintaining high quality and readability of its source code.
Moria uses [libmdbx] as the internal database engine.
This project is under active development and hasn't reached the alpha phase yet. For this reason there are no releases so far.

## Obtaining Source Code

To obtain the source code for the first time you need to install [Git](https://git-scm.com/) on your computer and
```shell
$ git clone --recurse-submodules https://github.com/HorizenLabs/moria.git
$ cd moria
```
We use some git [submodules] (which may eventually have their own submodules) : so after you've updated to the latest code with `git pull` remember to also update [submodules] with
```shell
$ git submodule update --init --recursive
```
## Building on Linux & MacOS

Ensure you have the following requirements installed :
- C++20 compatible compiler and its support libraries: [GCC](https://www.gnu.org/software/gcc/) >= 12 or [Clang](https://clang.llvm.org/) >= 13 (see [here](https://en.cppreference.com/w/cpp/compiler_support) the compatibility matrix)
- [CMake] >= 3.16.12
- [Perl](https://www.perl.org/) >= 5.x

Once the prerequisites are installed boostrap cmake by running
```shell
$ mkdir build
$ cd build
$ cmake [-DCMAKE_BUILD_TYPE="[Debug|Release|RelWithDebInfo|MinSizeRel]"]..
```
_On the very first run of this command the toolchain will download and build additional components like, for example, the Boost Library.
This operation may take some time depending on the capabilities of your hardware and your internet connection.
Please be patient._

If you're on linux and have both GCC and Clang installed CMAKE will use GCC by default. Should you want to force the build using clang simply export the
following variables before invoking cmake
```shell
$ export CC=/usr/bin/clang
$ export CXX=/usr/bin/clang++
$ cmake [-DCMAKE_BUILD_TYPE="[Debug|Release|RelWithDebInfo|MinSizeRel]"] ..
```
**Important** Should you already have built with GCC earlier remember do erase the `build` directory and re-create it.

Additional CMAKE options (specify with `-D<OPTION_NAME[:type]>=<value>`):

| OPTION_NAME          | Description                                        | Default |
|:---------------------|:---------------------------------------------------|:-------:|
| `ZEN_CORE_ONLY`      | Only build ZEN Core components                     |   OFF   |
| `ZEN_CLANG_COVERAGE` | **Clang** (only) instrumentation for code coverage |   OFF   |
| `ZEN_SANITIZE`       | Build instrumentation for sanitizers               |   OFF   |
| `ZEN_TESTS`          | Build unit / consensus tests                       |   ON    |

Then run the build itself
```shell
$ make -j
```
_Note about parallel builds using `-j`: if not specified the exact number of parallel tasks, the compiler will spawn as many
as the cores available. That may cause OOM errors if the build is executed on a host with a large number of cores but a relatively
small amount of RAM. To work around this, either specify `-jn` where `n` is the number of parallel tasks you want to allow or
remove `-j` completely. Typically, for Moria each compiler job requires up to 4GB of RAM. So if, for example, your total RAM is 16GB
then `-j4` should be OK, while `-j8` is probably not. It also means that you need a machine with at least 4GB RAM to compile Moria._

Now you can run the unit tests (if you have chosen to build them. There's one for `core` and one for `node`.
```shell
$ ./cmd/test/core_test
$ ./cmd/test/node_test
```
Along with tests also benchmarks are built. If you want to play with them run
```shell
$ ./cmd/benckmark/core_benchmarks
$ ./cmd/benchmark/node_benchmarks
```

## Building on Windows
**Note! Native Windows builds are maintained for compatibility/portability reasons.
However, due to the lack of 128-bit integers support by MSVC, execution performance may be slightly impacted when compared to *nix builds.**

To be able to build on Windows you have to ensure the following requirements are installed
- [Visual Studio] Build Tools >= 2019 16.9.2 : ensure your setup includes CMake support and Windows 10 SDK 
- Perl Language : either [Strawberry Perl](https://strawberryperl.com/) or [Active State Perl](https://www.activestate.com/products/perl/) are fine

If you're willing to use [Visual Studio] (Community Edition is fine) as your primary IDE then the build tools are already included in the setup package (still you have to ensure the required components are installed).
Alternatively you can use [VSCode] or [CLion]

For Visual Studio setups follow this instructions:
- Ensure you've cloned the project just as described [here](#obtaining-source-code)
- Open Visual Studio and select File -> Open -> Cmake...
- Browse the folder where you have cloned this repository and select the file CMakeLists.txt
- Let CMake cache generation complete : on first run this may take several minutes, depending on your hardware and internet connection capabilities,  as it will download and build additional components like, for example, Boost library.
- Solution explorer shows the project tree.
- To build simply `CTRL+Shift+B`
- Build files, libraries and executable binaries are written to `"${projectDir}\build\` If you want to change this path simply edit `CMakeSettings.json` file and choose an output directory which does not pollute the source directory tree (e.g. `%USERPROFILE%\.cmake-builds\${projectName}\`)

*We've deliberately chosen to force cmake generator to `Visual Studio 17 2022 Win64` even if it might result being slower than `Ninja`: fact is [Boost](https://www.boost.org/) libraries fail to build properly on windows using Ninja generator.*

### Memory compression on Windows 10/11

Windows 10/11 provide a _memory compression_ feature which makes available more RAM than what physically mounted at cost of extra CPU cycles to compress/decompress while accessing data. As MDBX is a memory mapped file this feature may impact overall performances. Is advisable to have memory compression off.
Use the following steps to detect/enable/disable memory compression:
* Open a PowerShell prompt with Admin privileges
* Run `Get-MMAgent` (check whether memory compression is enabled)
* To disable memory compression : `Disable-MMAgent -mc` and reboot
* To enable memory compression : `Enable-MMAgent -mc` and reboot

## Tree Map
This projects contains the following directory components:
* [`cmake`](./cmake)
  <br /> Where main cmake components are stored. Generally you don't need to edit anything there.
* [`cmd`](./cmd) 
  <br /> The basic source code of project's executable binaries (daemon and support tools).
  <br /> Nothing in this directory gets built when you choose the `ZEN_CORE_ONLY` build option
* [`doc`](./doc)
  <br /> The documentation area. No source code is allowed here
* [`third-party`](./third-party)
  <br /> Where most of the dependencies of the project are stored. Some directories may be bound to [submodules] while other may contain imported code.
* [`zen/core`](./zen/core)
  <br /> This module contains the heart of the Zen protocol logic.
  Source code within `core` is suitable for export (as a library) to third-party applications and cannot make use of C++ exceptions (build flags explicitly voids them)
* [`zen/node`](./zen/node)
  <br /> This module contains the database, the staged sync loop and other logic necessary to function as a Zen node.
  This module depends on the `core` module.

To simplify the building process cmake is configured to make use of GLOB lists of files. As a result a strict naming convention of files (see [Style Guide](#style-guide)).
In addition to that we establish two file names suffix (before extension) reservations:
* `_test` explicitly mark a file to be included in the unit tests target
* `_benchmark` explicitly mark a file to be included in the benchmarks target

## Style guide
We use standard C++20 programming language.
We adhere to [Google's C++ Style Guide] with the following differences:
- `C++20` rather than `C++17`
- `snake_case` for source and header file names (ISO) 
- `snake_case()` for function and variable names (ISO)
- `member_variable_` names must have underscore suffix
- prefixing variable names with initial abbreviation of underlying type (e.g `vector<char> vChar{}`) is highly discouraged
- classes and struct names must be in Pascal Case (`class FancyFoo`)
- prefer `using` instead of `typedef`
- `.cpp/.hpp` file extensions for C++ : `.c/.h` are reserved for C
- `using namespace foo` is allowed into source files (`.cpp`) but not inside headers
- Exceptions are allowed **only outside** the `core` library
- User-defined literals are allowed
- Maximum line length is 120, indentation is 4 spaces - see [.clang-format](.clang-format)
- Use `#pragma once` in the headers instead of the classic `#ifndef` guards.
- Comments MUST adhere to Doxygen formats (excluding inline ones)

Developers willing to contribute are strongly encouraged to take a thorough read of [this best practices about naming and layout](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#nl-naming-and-layout-suggestions)