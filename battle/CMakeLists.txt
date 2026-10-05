# The live battle simulation and its trained commander (battle/include/sovereign_battle).
# Plain C++17 with no Unreal and no rules core: core/CMakeLists.txt adds it, and the
# Unreal SovereignCore module compiles the same sources through generated wrappers.
add_library(sovereign_battle
  src/sim.cpp
  src/commander.cpp
  src/arena.cpp
)
target_include_directories(sovereign_battle PUBLIC include ${CMAKE_CURRENT_SOURCE_DIR}/../core/include)
if(MSVC)
  target_compile_options(sovereign_battle PRIVATE /W4 /permissive-)
  if(SOVEREIGN_WERROR)
    target_compile_options(sovereign_battle PRIVATE /WX)
  endif()
else()
  target_compile_options(sovereign_battle PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  if(SOVEREIGN_WERROR)
    target_compile_options(sovereign_battle PRIVATE -Werror)
  endif()
endif()

set(SOVEREIGN_BATTLE_AI "${CMAKE_CURRENT_SOURCE_DIR}/../data/battle_ai/commander.txt" CACHE FILEPATH "Trained battle commander")

add_executable(battle_train tools/battle_train.cpp)
target_link_libraries(battle_train PRIVATE sovereign_battle)

add_executable(battle_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/tests/test_main.cpp
  tests/test_sim.cpp
  tests/test_commander.cpp
)
target_include_directories(battle_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core/tests)
target_link_libraries(battle_tests PRIVATE sovereign_battle)
target_compile_definitions(battle_tests PRIVATE SOVEREIGN_BATTLE_AI="${SOVEREIGN_BATTLE_AI}")
add_test(NAME battle_tests COMMAND battle_tests)
