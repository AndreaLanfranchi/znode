# Znode - C++20 Bitcoin like node implementation

## Table of Contents

- [About Znode](#about-Znode)
- [Obtaining source code](#obtaining-source-code)
- [Building on Linux & macOS](#building-on-linux--macos)
- [Building on Windows](#building-on-windows)
- [Style Guide](#style-guide)
- [Don't reinvent the wheel !](#dont-reinvent-the-wheel-)
- [Detailed Documentation](./doc)

[CMake]: http://cmake.org
[Google's C++ Style Guide]: https://google.github.io/styleguide/cppguide.html
[libmdbx]: https://gitflic.ru/project/erthink/
[Visual Studio]: https://www.visualstudio.com/downloads
[VSCode]: https://www.visualstudio.com/downloads
[CLion]: https://www.jetbrains.com/clion/download/
[submodules]: https://git-scm.com/book/en/v2/Git-Tools-Submodules

## About Znode

Znode is mostly a greenfield C++(20+) implementation for a bitcoin-like node. The "Z" depicts most of the inspiration is taken from [Zcash](https://z.cash/) and [Horizen](https://www.horizen.io/) (for which I used to work).  
Main purpose of this work is to chase the efficiency frontier leveraging, at maximum extent possible, parallel programming and multitasking
while, at the same time, maintaining high software quality standards, coherent coding styles, and maximum readability.
Znode uses [libmdbx] as the internal database engine.
This project is under active development and hasn't reached the alpha phase yet. For this reason there are no releases so far.

## Obtaining Source Code

To obtain the source code for the first time you need to install [Git](https://git-scm.com/) on your computer and
```shell
$ git clone --recurse-submodules https://github.com/AndreaLanfranchi/znode.git
$ cd znode
```
We use some git [submodules] (which may eventually have their own submodules) : so after you've updated to the latest code with `git pull` remember to also update [submodules] with
```shell
$ git submodule update --init --recursive
```
## Building on Linux & macOS

Ensure you have the following requirements installed :
- C++20 compatible compiler and its support libraries: [GCC](https://www.gnu.org/software/gcc/) >= 13 or [Clang](https://clang.llvm.org/) >= 13 (see [here](https://en.cppreference.com/w/cpp/compiler_support) the compatibility matrix)
- [CMake] >= 3.16.12
- [Perl](https://www.perl.org/) >= 5.x

Once the prerequisites are installed boostrap cmake by running
```shell
$ mkdir build
$ cd build
$ cmake [-DCMAKE_BUILD_TYPE="[Debug|Release|RelWithDebInfo|MinSizeRel]"] ..
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

| OPTION_NAME            | Description                                        | Default |
|:-----------------------|:---------------------------------------------------|:-------:|
| `BUILD_CORE_ONLY`      | Only build Znode Core components                   |  `OFF`  |
| `BUILD_CLANG_COVERAGE` | **Clang** (only) instrumentation for code coverage |  `OFF`  |
| `BUILD_SANITIZE`       | Build instrumentation for sanitizers               |  `OFF`  |
| `BUILD_TESTS`          | Build unit / consensus siphash_tests                       |  `ON`   |

Then run the build itself
```shell
$ make -j
```
_Note about parallel builds using `-j`: if not specified the exact number of parallel tasks, the compiler will spawn as many
as the cores available. That may cause OOM errors if the build is executed on a host with a large number of cores but a relatively
small amount of RAM. To work around this, either specify `-jn` where `n` is the number of parallel tasks you want to allow or
remove `-j` completely. Typically, for Znode each compiler job requires up to 4GB of RAM. So if, for example, your total RAM is 16GB
then `-j4` should be OK, while `-j8` is probably not. It also means that you need a machine with at least 4GB RAM to compile Znode._

Now you can run the unit siphash_tests (if you have chosen to build them). There's one for `core` and one for `node`.
```shell
$ ./cmd/test/znode-core-test
$ ./cmd/test/znode-main-test
```
Along with siphash_tests also benchmarks are built. If you want to play with them run
```shell
$ ./cmd/benckmark/znode-core-benchmarks
$ ./cmd/benchmark/znode-infra-benchmarks
```

## Building on Windows
**Note! Native Windows builds are maintained for compatibility/portability reasons.
However, due to the lack of 128-bit integers support by MSVC, execution performance may be slightly impacted when compared to *nix builds.**

To be able to build on Windows you have to ensure the following requirements are installed
- [Visual Studio] Build Tools >= 2019 16.9.2 : ensure your setup includes CMake support and Windows 10 SDK 
- Perl Language : either [Strawberry Perl](https://strawberryperl.com/) or [Active ComponentStatus Perl](https://www.activestate.com/products/perl/) are fine

If you're willing to use [Visual Studio] (Community Edition is fine) as your primary IDE then the build tools are already included in the setup package (still you have to ensure the required components are installed).
Alternatively you can use [VSCode] or [CLion]

For Visual Studio setups follow these instructions:
- Ensure you've cloned the project just as described [here](#obtaining-source-code)
- Open Visual Studio and select File -> Open -> Cmake...
- Browse the folder where you have cloned this repository and select the file CMakeLists.txt
- Let CMake cache generation complete : on first run this may take several minutes, depending on your hardware and internet connection capabilities,  as it will download and build additional components like, for example, Boost library.
- Solution explorer shows the project tree.
- To build simply `CTRL+Shift+B`
- Build files, libraries and executable binaries are written to `"${projectDir}\build\` If you want to change this path simply edit `CMakeSettings.json` file and choose an output directory which does not pollute the source directory tree (e.g. `%USERPROFILE%\.cmake-builds\${projectName}\`)

*We've deliberately chosen to force cmake generator to `Visual Studio 17 2022 Win64` even if it might result being slower than `Ninja`: fact is [Boost](https://www.boost.org/) libraries fail to build properly on MSVC toolchain using Ninja generator.*

### Memory compression on Windows 10/11

Windows 10/11 provide a _memory compression_ feature which makes available more RAM than what physically mounted at cost of extra CPU cycles to compress/decompress while accessing data. As MDBX is a memory mapped file this feature may impact overall performances. Is advisable to have memory compression off.
Use the following steps to detect/enable/disable memory compression:
* Open a PowerShell prompt with Admin privileges
* Run `Get-MMAgent` (check whether memory compression is enabled)
* To disable memory compression : `Disable-MMAgent -mc` and reboot
* To enable memory compression : `Enable-MMAgent -mc` and reboot

## Style guide
We use standard C++20 programming language.
We adhere to [Google's C++ Style Guide] with the following differences:
- `C++20` rather than `C++17`
- `snake_case` for source and header file names (ISO) 
- `snake_case()` for function and variable names (ISO)
- `member_variable_` names **MUST** have underscore suffix
- prefixing variable names with initial abbreviation of underlying type (e.g `vector<char> vChar{}`) is highly discouraged
- classes and struct names **MUST** be in Pascal Case (`class FancyFoo`)
- prefer `using` instead of `typedef`
- `.cpp/.hpp` file extensions for C++ : `.c/.h` are reserved for C
- `using namespace foo` is allowed into source files but not inside headers unless limited in a reduced scope (i.e. inside a template class or function)
- Exceptions are allowed **only outside** the `core` library
- User-defined literals are allowed
- Maximum line length is 120, indentation is 4 spaces - see [.clang-format](.clang-format)
- Use `#pragma once` in the headers instead of the classic `#ifndef` guards.
- Comments **MUST** adhere to Doxygen formats (excluding inline ones)
- Avoid implicit conversions (e.g. `int` to `bool`)
- Avoid `auto` when the type is not immediately obvious (e.g. `auto foo = get_foo()`)
- Use `auto` when the type is immediately obvious (e.g. `auto foo = std::make_unique<Foo>()`)
- Prefer `if(ptr == nullptr)` over `if(!ptr)` for immediate clarity
- Prefer usage of monadic return values over simple `bool` (e.g. `std::optional<T>`, `std::variant<T, E>`, `std::expected<T, E>`)

Developers willing to contribute are strongly encouraged to take a thorough read of [this best practices about naming and layout](https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md#nl-naming-and-layout-suggestions)

## Don't reinvent the wheel !
While it is often tempting to write your own implementation of a well known algorithm or data structure, we strongly encourage you to use the ones provided by the standard library or by one of the following well-known and widely used libraries:
- [Boost](https://www.boost.org/).
- [Google's Abseil](https://abseil.io/).
- [Microsoft's GSL](https:://github.com/microsoft/GSL).
- [OpenSSL](https://www.openssl.org/).

If you can't find what you need there, maybe a quick search on [GitHub](https://github.com) might help finding a good library which already has everything you need.
A good starting point might be this [awesome list of C++ libraries](https://github.com/fffaraz/awesome-cpp).

Consider writing your own implementation as last resort and only if you can't find anything suitable.
