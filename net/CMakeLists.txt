# Online play (net/include/sovereign_net): the lockstep session over the core's command log.
# Plain C++17 over the rules core: core/CMakeLists.txt adds it. The TCP links are a separate
# library so engine builds can bring their own transport.
find_package(Threads REQUIRED)

add_library(sovereign_net
  src/protocol.cpp
  src/host.cpp
  src/client.cpp
)
target_include_directories(sovereign_net PUBLIC include)
target_link_libraries(sovereign_net PUBLIC sovereign_core Threads::Threads)

add_library(sovereign_net_tcp src/tcp.cpp)
target_link_libraries(sovereign_net_tcp PUBLIC sovereign_net)
if(WIN32)
  target_link_libraries(sovereign_net_tcp PRIVATE ws2_32)
endif()

foreach(t sovereign_net sovereign_net_tcp)
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

add_executable(sovnet tools/sovnet.cpp)
target_link_libraries(sovnet PRIVATE sovereign_net_tcp)

add_executable(net_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/../core/tests/test_main.cpp
  tests/test_session.cpp
)
target_include_directories(net_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core/tests)
target_link_libraries(net_tests PRIVATE sovereign_net_tcp)
target_compile_definitions(net_tests PRIVATE SOVEREIGN_RULES_DIR="${SOVEREIGN_RULES_DIR}")
add_test(NAME net_tests COMMAND net_tests)
