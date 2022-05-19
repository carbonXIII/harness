add_executable(pi_server main.cpp)

target_include_directories(pi_server PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(pi_server PRIVATE fmt)
