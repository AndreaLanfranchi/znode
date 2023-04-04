# TO DOs
- [Documents Index](README.md)

## std::format
While this should be part of C++20 standard yet GCC and CLANG (at the versions currently - 2023 - available) do consider
this as experimental. Still using Boost::format
## std::expected
Will be available in C++23. As a result we make use of monadic from [tl/expected](https://github.com/TartanLlama/expected).
std::optional is not an "option" as it looses info about reason for missed value generation
## OpenSSL 3
Hunter does not support it yet. See WIP at https://github.com/cpp-pm/hunter/pull/668