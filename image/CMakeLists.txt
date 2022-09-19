include(FetchContent)

find_program(PACKER_COMMAND packer)
find_program(DOCKER_COMMAND docker)

# packer -> Docker image harness:base -> Docker image harness:builder
add_custom_command(
  OUTPUT rootfs.tar.gz
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS base.pkr.hcl Dockerfile
  COMMAND ${PACKER_COMMAND} build ${CMAKE_CURRENT_SOURCE_DIR}/base.pkr.hcl)

file(COPY_FILE .dockerignore ${CMAKE_CURRENT_BINARY_DIR}/.dockerignore)

add_custom_target(builder_image
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/rootfs.tar.gz ${CMAKE_CURRENT_BINARY_DIR}/.dockerignore
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND docker build . -f ${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile -t harness:builder)

# build artifacts under build/image/build via harness:builder
ExternalProject_add(
  pi_builder
  DEPENDS builder_image
  EXCLUDE_FROM_ALL TRUE
  BUILD_ALWAYS TRUE
  STEP_TARGETS build
  SOURCE_DIR ${CMAKE_SOURCE_DIR}
  CONFIGURE_COMMAND ${DOCKER_COMMAND} run -v ${CMAKE_SOURCE_DIR}:/src -v <BINARY_DIR>:/build -w "/build" -it harness:builder cmake /src
  BUILD_COMMAND ${DOCKER_COMMAND} run -v ${CMAKE_SOURCE_DIR}:/src -v <BINARY_DIR>:/build -w "/build" -it harness:builder cmake --build .
  BUILD_BYPRODUCTS <BINARY_DIR>/pi/pi-server
  INSTALL_COMMAND ""
)

# grab raspberry pi boot files under build/image/firmware via svn
FetchContent_Populate(
  firmware
  SVN_REPOSITORY https://github.com/raspberrypi/firmware/tags/1.20200212/boot
  SOURCE_DIR firmware
)

# final stage artifact is the writeable image (harness.img)
# - run this if any of the previous stages get run
add_custom_command(
  OUTPUT harness.img
  DEPENDS image.pkr.hcl ${CMAKE_CURRENT_BINARY_DIR}/rootfs.tar.gz pi_builder-build
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${PACKER_COMMAND} build
    -var "input_image_path=${CMAKE_CURRENT_BINARY_DIR}/rootfs.tar.gz"
    -var "project_root=${CMAKE_SOURCE_DIR}"
    ${CMAKE_CURRENT_SOURCE_DIR}/image.pkr.hcl
)

add_custom_target(image DEPENDS harness.img)
