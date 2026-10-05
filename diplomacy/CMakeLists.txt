# The dialogue layer for language-model diplomacy (diplomacy/include/sovereign_diplomacy).
# Plain C++17 over the rules core: core/CMakeLists.txt adds it, and the Unreal SovereignCore
# module compiles the same sources through generated wrappers. The socket client is a separate
# library so engine builds can use their own HTTP stack.
add_library(sovereign_diplomacy
  src/persona.cpp
  src/parse.cpp
  src/safety.cpp
  src/scripted.cpp
  src/llama.cpp
  src/conversation.cpp
)
target_include_directories(sovereign_diplomacy PUBLIC include)
target_link_libraries(sovereign_diplomacy PUBLIC sovereign_core)

add_library(sovereign_diplomacy_http src/http.cpp)
target_link_libraries(sovereign_diplomacy_http PUBLIC sovereign_diplomacy)
if(WIN32)
  target_link_libraries(sovereign_diplomacy_http PRIVATE ws2_32)
endif()

foreach(t sovereign_diplomacy sovereign_diplomacy_http)
  if(MSVC)
    target_compile_options(${t} PRIVATE /W4 /permissive-)
    if(SOVEREIGN_WERROR)
      target_compile_options(${t} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${t} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(SOVEREIGN_WERROR)
      target_compile_options(${t} PRIVATE -Werror)
    endif()
  endif()
endforeach()

add_executable(diplo_chat tools/diplo_chat.cpp)
target_link_libraries(diplo_chat PRIVATE sovereign_diplomacy_http)

add_executable(diplomacy_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/tests/test_main.cpp
  tests/test_dialogue.cpp
)
target_include_directories(diplomacy_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core/tests)
target_link_libraries(diplomacy_tests PRIVATE sovereign_diplomacy_http)
target_compile_definitions(diplomacy_tests PRIVATE SOVEREIGN_RULES_DIR="${SOVEREIGN_RULES_DIR}")
add_test(NAME diplomacy_tests COMMAND diplomacy_tests)
