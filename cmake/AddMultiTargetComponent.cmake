# Based on: https://www.vinnie.work/blog/2020-11-17-cmake-eval/

include(ExternalProject)

function(add_subdirectory_for_toolchain dir toolchainTuple)
  if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL ${toolchainTuple})
    add_subdirectory(${dir})
  endif()
endfunction()

function(trigger_toolchain_build toolchainTuple hostTuple)
  if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL ${hostTuple})
    set(projectToolchainTuple rebuild-${toolchainTuple})
    set(toolchainFile ${CMAKE_SOURCE_DIR}/cmake/toolchains/${toolchainTuple}.cmake)
    set(outputDir ${CMAKE_BINARY_DIR}/${toolchainTuple})

    ExternalProject_Add(${projectToolchainTuple}
      PREFIX ${outputDir}
      SOURCE_DIR ${CMAKE_SOURCE_DIR}
      BINARY_DIR ${outputDir}
      # CONFIGURE_COMMAND ""
      CMAKE_GENERATOR ${CMAKE_GENERATOR}
      CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${toolchainFile} -B ${outputDir}
      INSTALL_COMMAND ""
    )
  endif()
endfunction()
