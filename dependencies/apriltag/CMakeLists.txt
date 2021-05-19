cmake_minimum_required(VERSION 3.1)
project(apriltag)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")
SET(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
SET(CMAKE_RELWITHDEBINFO_POSTFIX "rd" CACHE STRING "add a postfix, usually empty on windows")
SET(CMAKE_MINSIZEREL_POSTFIX "s" CACHE STRING "add a postfix, usually empty on windows")

include_directories(.)
aux_source_directory(common COMMON_SRC)
set(APRILTAG_SRCS apriltag.c apriltag_pose.c apriltag_quad_thresh.c)
file(GLOB TAG_FILES ${CMAKE_SOURCE_DIR}/tag*.c)
# add_library(${PROJECT_NAME} SHARED ${APRILTAG_SRCS} ${COMMON_SRC} ${TAG_FILES})
add_library(${PROJECT_NAME} ${APRILTAG_SRCS} ${COMMON_SRC} ${TAG_FILES})
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads m)

