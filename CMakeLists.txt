cmake_minimum_required(VERSION 3.10)

project(loadbalancer)

set(PUBLIC_INCLUDE_DIRECTORIES include/)
set(PRIVATE_INCLUDE_DIRECTORIES src/)

file(GLOB_RECURSE SOURCE_FILES "src/*.c" "src/*.h")
file(GLOB_RECURSE INCLUDE_FILES "include/*.h")

add_executable(${PROJECT_NAME} ${INCLUDE_FILES} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${PUBLIC_INCLUDE_DIRECTORIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${PRIVATE_INCLUDE_DIRECTORIES})
target_include_directories(${PROJECT_NAME} PUBLIC lib/sglib/)
target_include_directories(${PROJECT_NAME} PUBLIC lib/packetvisor/include/)
target_link_libraries(${PROJECT_NAME} ${CMAKE_SOURCE_DIR}/lib/packetvisor/libpv.a)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Werror)

set(LIBPV_PATH "${CMAKE_SOURCE_DIR}/lib/packetvisor/libpv.a")

add_custom_command(
    OUTPUT ${LIBPV_PATH}
    COMMAND make libpv.a
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/packetvisor
    )

add_custom_target(
    libpv
    DEPENDS ${LIBPV_PATH}
)

add_dependencies(
    ${PROJECT_NAME}
    libpv
)

install(
    TARGETS ${PROJECT_NAME}
)
