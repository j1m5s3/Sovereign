# The weekly challenge's server board (board/include/sovereign_board).
# Plain C++17 over the rules core: core/CMakeLists.txt adds it. The TCP links are a separate
# library so engine builds can bring their own transport.
find_package(Threads REQUIRED)

add_library(sovereign_board
  src/board.cpp
  src/protocol.cpp
)
target_include_directories(sovereign_board PUBLIC include)
target_link_libraries(sovereign_board PUBLIC sovereign_core Threads::Threads)

add_library(sovereign_board_tcp src/tcp.cpp)
target_link_libraries(sovereign_board_tcp PUBLIC sovereign_board)
if(WIN32)
  target_link_libraries(sovereign_board_tcp PRIVATE ws2_32)
endif()

foreach(t sovereign_board sovereign_board_tcp)
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

add_executable(sovboard tools/sovboard.cpp)
target_link_libraries(sovboard PRIVATE sovereign_board_tcp)

add_executable(board_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/tests/test_main.cpp
  tests/test_board.cpp
)
target_include_directories(board_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core/tests)
target_link_libraries(board_tests PRIVATE sovereign_board_tcp)
target_compile_definitions(board_tests PRIVATE SOVEREIGN_RULES_DIR="${SOVEREIGN_RULES_DIR}")
add_test(NAME board_tests COMMAND board_tests)
