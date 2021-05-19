cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

function(get_ndk NDK)
   set(NDK_ $ENV{NDK_ROOT})
   if (NDK_ STREQUAL "")
      set(NDK_ $ENV{NDK})
   endif()
   if (NDK_ STREQUAL "")
      message(FATAL_ERROR "Please set the NDK_ROOT or NDK environment variable to point to the NDK base directory.")
   endif()
   get_filename_component(NDK_ ${NDK_} REALPATH)
   if(IS_DIRECTORY ${NDK_})
      set(${NDK} "${NDK_}" PARENT_SCOPE)
   else()
      message(FATAL_ERROR "Please set the NDK_ROOT environment variable to point to the NDK base directory (${NDK}).")
   endif()
endfunction()

project (tbb CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_VERBOSE_MAKEFILE ON)

get_ndk(NDK)
message(STATUS "Found NDK ${NDK}")

# file(GLOB tbb_src "${CMAKE_CURRENT_SOURCE_DIR}/src/tbb/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/old/*.cpp")
# list(APPEND tbb_src ${CMAKE_CURRENT_SOURCE_DIR}/src/rml/client/rml_tbb.cpp)
# file(GLOB to_remove "${CMAKE_CURRENT_SOURCE_DIR}/src/old/test*.cpp")
# list(REMOVE_ITEM tbb_src ${to_remove})
if (${ANDROID_ABI} STREQUAL "x86_64")
   set(ARCH "arch-x86_64")
   set(ARCH_PREFIX "x86_64-linux-android")
   set(ARCH_ONLY "x86_64")
elseif (${ANDROID_ABI} STREQUAL "x86")
   set(ARCH "arch-x86")
   set(ARCH_PREFIX "i686-linux-android")
   set(ARCH_ONLY "x86")
elseif (${ANDROID_ABI} STREQUAL "arm64-v8a")
   set(ARCH "arch-arm64")
   set(ARCH_PREFIX "aarch64-linux-android")
   set(ARCH_ONLY "arm64-v8a")
elseif (${ANDROID_ABI} STREQUAL "armeabi-v7a")
   set(ARCH "arch-arm")
   set(ARCH_PREFIX "arm-linux-androideabi")
   set(ARCH_ONLY "armeabi-v7a")
endif()
set(PLATFORM "${NDK}/platforms/android-${ANDROID_NATIVE_API_LEVEL}/${ARCH}")
set(SYSROOT "${NDK}/sysroot")

message(STATUS "ANDROID_ABI=${ANDROID_ABI}")
message(STATUS "ANDROID_SYSROOT=${ANDROID_SYSROOT}")
message(STATUS "SYSROOT=${SYSROOT}")
message(STATUS "ANDROID_NATIVE_API_LEVEL=${ANDROID_NATIVE_API_LEVEL}")

set(LIBS_DIR ${CMAKE_SOURCE_DIR}/../../app/c++/static-libs/${ANDROID_ABI})
set(OUTPUT_BINDIR ${LIBS_DIR}/bin)
set(OUTPUT_LIBDIR ${LIBS_DIR})

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})

message(STATUS "Output to: ${OUTPUT_LIBDIR}")

SET(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
make_directory(${OUTPUT_LIBDIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})

# Change the default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)


set(TBB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/tbb")
set(RML_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/rml/client")
#From tbb/build/Makefile.tbb
set(TBB_SRC ${TBB_SOURCE_DIR}/concurrent_hash_map.cpp ${TBB_SOURCE_DIR}/concurrent_queue.cpp ${TBB_SOURCE_DIR}/concurrent_vector.cpp
    ${TBB_SOURCE_DIR}/dynamic_link.cpp ${TBB_SOURCE_DIR}/itt_notify.cpp ${TBB_SOURCE_DIR}/cache_aligned_allocator.cpp
	 ${TBB_SOURCE_DIR}/pipeline.cpp ${TBB_SOURCE_DIR}/queuing_mutex.cpp ${TBB_SOURCE_DIR}/queuing_rw_mutex.cpp ${TBB_SOURCE_DIR}/reader_writer_lock.cpp
	 ${TBB_SOURCE_DIR}/spin_rw_mutex.cpp ${TBB_SOURCE_DIR}/x86_rtm_rw_mutex.cpp ${TBB_SOURCE_DIR}/spin_mutex.cpp ${TBB_SOURCE_DIR}/critical_section.cpp
    ${TBB_SOURCE_DIR}/mutex.cpp ${TBB_SOURCE_DIR}/recursive_mutex.cpp ${TBB_SOURCE_DIR}/condition_variable.cpp ${TBB_SOURCE_DIR}/tbb_thread.cpp
	 ${TBB_SOURCE_DIR}/concurrent_monitor.cpp ${TBB_SOURCE_DIR}/semaphore.cpp ${TBB_SOURCE_DIR}/private_server.cpp ${RML_SOURCE_DIR}/rml_tbb.cpp
	 ${TBB_SOURCE_DIR}/tbb_misc.cpp ${TBB_SOURCE_DIR}/tbb_misc_ex.cpp ${TBB_SOURCE_DIR}/task.cpp ${TBB_SOURCE_DIR}/task_group_context.cpp
	 ${TBB_SOURCE_DIR}/governor.cpp ${TBB_SOURCE_DIR}/market.cpp ${TBB_SOURCE_DIR}/arena.cpp ${TBB_SOURCE_DIR}/scheduler.cpp ${TBB_SOURCE_DIR}/observer_proxy.cpp
    ${TBB_SOURCE_DIR}/tbb_statistics.cpp ${TBB_SOURCE_DIR}/tbb_main.cpp)
# set(TBB_FLAGS "-MMD")
list(APPEND TBB_FLAGS -D_GLIBCXX_HAVE_FENV_H)
list(APPEND TBB_FLAGS --sysroot=${SYSROOT})
list(APPEND TBB_FLAGS -isystem ${ANDROID_SYSROOT}/usr/include/${ARCH_PREFIX})
list(APPEND TBB_FLAGS -D__ANDROID_API__=${ANDROID_NATIVE_API_LEVEL})
list(APPEND TBB_FLAGS -fpic)
list(APPEND TBB_FLAGS -fPIE)
list(APPEND TBB_FLAGS -fPIC)
list(APPEND TBB_FLAGS -fstack-protector)
list(APPEND TBB_FLAGS -Wformat)
list(APPEND TBB_FLAGS -Wformat-security)
list(APPEND TBB_FLAGS -D_FORTIFY_SOURCE=2)
list(APPEND TBB_FLAGS -D__TBB_BUILD=1)
list(APPEND TBB_FLAGS -Wall)
list(APPEND TBB_FLAGS -Wno-parentheses)
list(APPEND TBB_FLAGS -Wno-non-virtual-dtor)
list(APPEND TBB_FLAGS -Wno-unused-private-field)
list(APPEND TBB_FLAGS -frtti)
list(APPEND TBB_FLAGS -fexceptions)
# list(APPEND TBB_FLAGS "-I${PROJECT_SOURCE_DIR}/include")

set(TBB_INCLUDES "${PROJECT_SOURCE_DIR}/src/")
# list(APPEND TBB_INCLUDES "${PROJECT_SOURCE_DIR}/include")
list(APPEND TBB_INCLUDES "${PROJECT_SOURCE_DIR}/src/rml/include")
list(APPEND TBB_INCLUDES "${NDK}/sources/cxx-stl/llvm-libc++/include")
#list(APPEND TBB_INCLUDES "${SYSROOT}/usr/include/")
#list(APPEND TBB_INCLUDES "${SYSROOT}/usr/include/${ARCH_PREFIX}")
#list(APPEND TBB_INCLUDES "${NDK}/sources/cxx-stl/llvm-libc++/include")

message(STATUS ${TBB_INCLUDES})

set(TBB_LINK_FLAGS "-L${PLATFORM}/usr/lib -L${NDK}/sources/cxx-stl/llvm-libc++/libs/${ARCH_ONLY} --sysroot=${PLATFORM} -fpic -fPIC")
set(TBB_LINK_LIBS "dl")
list(APPEND TBB_LINK_LIBS "c++_shared")
list(APPEND TBB_LINK_LIBS "c++abi")

set(MALLOC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/tbbmalloc")
set(MALLOC_SRC
  ${MALLOC_SOURCE_DIR}/backend.cpp ${MALLOC_SOURCE_DIR}/large_objects.cpp ${MALLOC_SOURCE_DIR}/backref.cpp ${MALLOC_SOURCE_DIR}/tbbmalloc.cpp
  ${TBB_SOURCE_DIR}/itt_notify.cpp ${MALLOC_SOURCE_DIR}/frontend.cpp )
# set(MALLOC_FLAGS "-MMD")
list(APPEND MALLOC_FLAGS -D_GLIBCXX_HAVE_FENV_H)
list(APPEND MALLOC_FLAGS --sysroot=${SYSROOT})
list(APPEND MALLOC_FLAGS -isystem ${SYSROOT}/usr/include/${ARCH_PREFIX})
list(APPEND MALLOC_FLAGS -D__ANDROID_API__=${ANDROID_NATIVE_API_LEVEL})
list(APPEND MALLOC_FLAGS -fpic)
list(APPEND MALLOC_FLAGS -fPIC)
list(APPEND MALLOC_FLAGS -D__TBBMALLOC_BUILD=1)
list(APPEND MALLOC_FLAGS -Wall)
list(APPEND MALLOC_FLAGS -Wno-parentheses)
list(APPEND MALLOC_FLAGS -Wno-non-virtual-dtor)
list(APPEND MALLOC_FLAGS -Wno-unused-private-field)
list(APPEND MALLOC_FLAGS -fno-rtti)
list(APPEND MALLOC_FLAGS -fno-exceptions)

set(MALLOC_INCLUDES "${PROJECT_SOURCE_DIR}/src/")
# list(APPEND MALLOC_INCLUDES "${PROJECT_SOURCE_DIR}/include")
list(APPEND MALLOC_INCLUDES "${PROJECT_SOURCE_DIR}/src/rml/include")
# list(APPEND MALLOC_INCLUDES "${SYSROOT}/usr/include/${ARCH_PREFIX}")
list(APPEND MALLOC_INCLUDES "${NDK}/sources/cxx-stl/llvm-libc++/include")

# add_custom_command(OUTPUT tbb.def
#     COMMAND ${CMAKE_CXX_COMPILER} -xc++ -E ${CMAKE_CURRENT_SOURCE_DIR}/src/tbb/${ARCH_PREFIX}-tbb-export.def  -I ${CMAKE_CURRENT_SOURCE_DIR}/include -o tbb.def
#     MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/src/tbb/${ARCH_PREFIX}-tbb-export.def
#     COMMENT "Preprocessing tbb.def"
#   )

#   add_custom_command(OUTPUT tbbmalloc.def
#     COMMAND ${CMAKE_CXX_COMPILER} -xc++ -E ${CMAKE_CURRENT_SOURCE_DIR}/src/tbbmalloc/${ARCH_PREFIX}-tbbmalloc-export.def  -I ${CMAKE_CURRENT_SOURCE_DIR}/include -o   tbbmalloc.def
#     MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/src/tbbmalloc/${ARCH_PREFIX}-tbbmalloc-export.def
#     COMMENT "Preprocessing tbbmalloc.def"
#   )

# set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
# find_package( Threads REQUIRED )
# MESSAGE(STATUS "Threads Lib: " ${CMAKE_THREAD_LIBS_INIT})
# MESSAGE(STATUS "Using PTHreads?: "  ${CMAKE_USE_PTHREADS_INIT})
# if (CMAKE_USE_PTHREADS_INIT)
#    list(APPEND TBB_FLAGS "-DUSE_PTHREAD")
#    list(APPEND MALLOC_FLAGS "-DUSE_PTHREAD")
#    list(APPEND TBB_LINK_LIBS ${CMAKE_THREAD_LIBS_INIT})
# endif()
list(APPEND TBB_FLAGS "-DUSE_PTHREAD")
list(APPEND MALLOC_FLAGS "-DUSE_PTHREAD")


cmake_host_system_information(RESULT _HOST_ QUERY "HOSTNAME")
#cmake_host_system_information(RESULT OS QUERY "OS_NAME")
set(_OS_ "${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR}")
set(_CLANGVER_ "${CMAKE_CXX_COMPILER_VERSION}")
set(_NDK_ "${NDK}")
set(_LINKER_ "${CMAKE_EXE_LINKER_FLAGS} ${TBB_LINK_FLAGS}")
set(_ARCH_ "${ANDROID_ABI}")
set(_ANDROIDVER_ "${ANDROID_NATIVE_API_LEVEL}")
# list_to_string(_flags_, " ", TBB_FLAGS)
foreach(_flag_ ${TBB_FLAGS})
   string(CONCAT _flags_ "${_flag_}")
endforeach()
# list_to_string(_includes_, " ", TBB_INCLUDES)
set(_COMPILE_ "clang++ ${_includes_} ${_flags_}")
string(TIMESTAMP _DATETIME_ UTC)
configure_file("${PROJECT_SOURCE_DIR}/tbb-version_string" "${PROJECT_SOURCE_DIR}/src/version_string.ver")

# add_custom_target(tbb_def_files DEPENDS tbb.def tbbmalloc.def)
set(CMAKE_EXE_LINKER_FLAGS ${TBB_LINK_FLAGS})
add_library(tbb STATIC ${TBB_SRC})
target_compile_options(tbb PRIVATE ${TBB_FLAGS})
target_include_directories(tbb PRIVATE ${TBB_INCLUDES})
target_include_directories(tbb PUBLIC "${PROJECT_SOURCE_DIR}/include")
# set_target_properties(tbb PROPERTIES
#                       ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${ARCH_ONLY}"
#                       LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${ARCH_ONLY}"
#                       RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${ARCH_ONLY}" )
target_link_libraries(tbb ${TBB_LINK_LIBS})
#target_link_options(tbb INTERFACE ${TBB_LINK_FLAGS})

add_library(tbbmalloc STATIC ${MALLOC_SRC})
target_compile_options(tbbmalloc PRIVATE ${MALLOC_FLAGS} )
target_include_directories(tbbmalloc PRIVATE ${MALLOC_INCLUDES})
target_include_directories(tbbmalloc PUBLIC "${PROJECT_SOURCE_DIR}/include")
# set_target_properties(tbbmalloc PROPERTIES
#                       ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${ARCH_ONLY}"
#                       LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${ARCH_ONLY}"
#                       RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${ARCH_ONLY}" )
