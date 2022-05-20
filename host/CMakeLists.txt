find_package(SDL2 REQUIRED)

add_executable(harness main.cpp)
target_include_directories(harness PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/external/asio/asio/include
)
target_link_libraries(harness PRIVATE fmt SDL2)
