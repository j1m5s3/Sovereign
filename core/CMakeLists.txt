# Sovereign rules core: plain C++ with no Unreal dependency (engine doc).
# Builds standalone for headless tests and AI self-play; the Unreal project
# later compiles the same sources as a module.
cmake_minimum_required(VERSION 3.20)
project(SovereignCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SOVEREIGN_WERROR "Treat warnings as errors" ON)

add_library(sovereign_core
  src/city.cpp
  src/research.cpp
  src/improvements.cpp
  src/combat.cpp
  src/barbarians.cpp
  src/fixed.cpp
  src/game.cpp
  src/hex.cpp
  src/json.cpp
  src/mapgen.cpp
  src/modifiers.cpp
  src/rng.cpp
  src/rules.cpp
  src/serialize.cpp
  src/state.cpp
)
target_include_directories(sovereign_core PUBLIC include)

if(MSVC)
  target_compile_options(sovereign_core PRIVATE /W4 /permissive- /fp:strict)
  if(SOVEREIGN_WERROR)
    target_compile_options(sovereign_core PRIVATE /WX)
  endif()
else()
  # -ffp-contract=off: no fused multiply-add surprises if a float ever slips in.
  target_compile_options(sovereign_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -ffp-contract=off)
  if(SOVEREIGN_WERROR)
    target_compile_options(sovereign_core PRIVATE -Werror)
  endif()
endif()

set(SOVEREIGN_RULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../data/rules" CACHE PATH "Rules data directory")

add_executable(sovsim tools/sovsim.cpp)
target_link_libraries(sovsim PRIVATE sovereign_core)

enable_testing()
add_executable(sovereign_tests
  tests/test_main.cpp
  tests/test_foundations.cpp
  tests/test_rules.cpp
  tests/test_map.cpp
  tests/test_game.cpp
  tests/test_save.cpp
  tests/test_city.cpp
  tests/test_research.cpp
  tests/test_improvements.cpp
  tests/test_combat.cpp
  tests/test_city_combat.cpp
)
target_link_libraries(sovereign_tests PRIVATE sovereign_core)
target_compile_definitions(sovereign_tests PRIVATE
  SOVEREIGN_RULES_DIR="${SOVEREIGN_RULES_DIR}"
  SOVEREIGN_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden")
add_test(NAME sovereign_tests COMMAND sovereign_tests)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME no_floats_in_core
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/check_core_rules.py)
  add_test(NAME generated_rules_current
           COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/rules_gen/gen_rules.py --check)
endif()
