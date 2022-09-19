find_package(Threads REQUIRED)

add_executable(harness_server main.cpp)
target_include_directories(harness_server PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/external/asio/asio/include
)
target_link_libraries(harness_server PRIVATE fmt Threads::Threads)

add_executable(echo echo.cpp)
target_include_directories(echo PRIVATE ${CMAKE_SOURCE_DIR}/external/asio/asio/include)
target_link_libraries(echo PRIVATE fmt Threads::Threads)
