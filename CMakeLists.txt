cmake_minimum_required(VERSION 3.10)

project(loadbalancer)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK_PKG REQUIRED IMPORTED_TARGET libdpdk)

set(LIBPV_PATH "${CMAKE_SOURCE_DIR}/lib/packetvisor/libpv.a")
add_custom_target(
    libpv
    COMMAND make DEBUG=0 libpv.a
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/packetvisor
)

set(PYPV_PATH "${CMAKE_SOURCE_DIR}/lib/packetvisor/bin/pv")
add_custom_target(
    pypv
    COMMAND make DEBUG=0 bin/pv
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/packetvisor
    )
add_custom_target(
    copy_pypv
    BYPRODUCTS ${CMAKE_BINARY_DIR}/pv
    COMMAND cp ${PYPV_PATH} ${CMAKE_BINARY_DIR}/pv
    DEPENDS pypv
)

set(LIBCL_PATH "${CMAKE_SOURCE_DIR}/lib/libcollection/libcl.a")
add_custom_target(
    libcl
    COMMAND make DEBUG=0 libcl.a
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/libcollection
)

set(PUBLIC_INCLUDE_DIRECTORIES include/)
set(PRIVATE_INCLUDE_DIRECTORIES src/)

file(GLOB_RECURSE SOURCE_FILES "src/*.c" "src/*.h")
file(GLOB_RECURSE INCLUDE_FILES "include/*.h")

add_executable(${PROJECT_NAME} ${INCLUDE_FILES} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${PUBLIC_INCLUDE_DIRECTORIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(${PROJECT_NAME} PUBLIC lib/libcollection/include/)
target_include_directories(${PROJECT_NAME} PUBLIC lib/packetvisor/include/)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Werror)

add_dependencies(
    ${PROJECT_NAME}
    libpv
    copy_pypv
    libcl
)

target_link_libraries(
    ${PROJECT_NAME}
    ${LIBPV_PATH}
    ${LIBCL_PATH}
    PkgConfig::DPDK_PKG
)

file(
    COPY ${CMAKE_SOURCE_DIR}/config.yaml
    DESTINATION ${CMAKE_BINARY_DIR})

install(
    TARGETS ${PROJECT_NAME}
)
