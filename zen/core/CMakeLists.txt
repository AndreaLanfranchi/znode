#[[
   Copyright 2022 The Silkworm Authors
   Copyright 2023 Horizen Labs

   Distributed under the MIT software license, see the accompanying
   file COPYING or http://www.opensource.org/licenses/mit-license.php.
]]

message(CHECK_START "Looking for required packages ...")
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)


file(GLOB_RECURSE ZEN_CORE_SRC CONFIGURE_DEPENDS "*.cpp" "*.hpp" "*.c" "*.h")
list(FILTER ZEN_CORE_SRC EXCLUDE REGEX "_test\\.cpp$")
list(FILTER ZEN_CORE_SRC EXCLUDE REGEX "_benchmark\\.cpp$")

add_library(zen_core ${ZEN_CORE_SRC} common/capped_set.hpp)
target_include_directories(zen_core PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(zen_core SYSTEM PUBLIC ${ZEN_MAIN_DIR}/third-party/expected/include)
set_target_properties(zen_core PROPERTIES LINKER_LANGUAGE CXX)

# No exceptions here
target_compile_definitions(zen_core PRIVATE BOOST_NO_EXCEPTIONS)
if (MSVC)
    target_compile_options(zen_core PRIVATE /EHa- /EHsc)
else ()
    target_compile_options(zen_core PRIVATE -fno-exceptions)
endif ()

set(ZEN_CORE_PUBLIC_LIBS intx::intx Microsoft.GSL::GSL nlohmann_json OpenSSL::Crypto)
set(ZEN_CORE_PRIVATE_LIBS "")

if(MSVC)
    # See https://github.com/microsoft/vcpkg/issues/2621#issuecomment-359374703
    list(APPEND ZEN_CORE_PRIVATE_LIBS Ws2_32.lib)
endif(MSVC)

target_link_libraries(zen_core PUBLIC ${ZEN_CORE_PUBLIC_LIBS} PRIVATE ${ZEN_CORE_PRIVATE_LIBS})
