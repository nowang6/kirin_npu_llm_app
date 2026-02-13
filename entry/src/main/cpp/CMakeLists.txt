# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(cannit)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

ADD_DEFINITIONS(-DVK_USE_PLATFORM_OHOS=1)
ADD_DEFINITIONS(-DVK_NO_PROTOTYPES=1)

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${NATIVERENDER_ROOT_PATH}/include/common)

add_library(hiai_llm_engine SHARED IMPORTED)
set_target_properties(hiai_llm_engine PROPERTIES IMPORTED_LOCATION
    ${NATIVERENDER_ROOT_PATH}/lib64/libhiai_llm_engine.so)

add_library(entry SHARED llm_demo.cpp)
target_link_libraries(entry PUBLIC libace_napi.z.so
    libhilog_ndk.z.so
    librawfile.z.so
    hiai_llm_engine)