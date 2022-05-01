cmake_minimum_required(VERSION 3.0)
project(khttpd)
file(GLOB KHTTPD_SOURCE_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp)
file(GLOB KHTTPD_INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/../include/*.h)
set(USE_CXX ON)
if (NOT KASYNC_DIR) 
 message(FATAL "config -DKASYNC_DIR")
endif()
add_subdirectory(${KASYNC_DIR}/build ${CMAKE_CURRENT_SOURCE_DIR}/kasync EXCLUDE_FROM_ALL)
#include_directories(${KASYNC_DIR}/build)


set(KHTTPD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)

#message(STATUS ${KASYNC_SOURCE_FILES})
list(APPEND KHTTPD_SOURCE_FILES ${KASYNC_SOURCE_FILES})
list(APPEND KHTTPD_INCLUDE_DIR ${KASYNC_INCLUDE_DIR})
list(APPEND KHTTPD_INCLUDE_FILES ${KASYNC_INCLUDE_FILES})
set(KHTTPD_LIBS ${KASYNC_LIBS})

add_library(khttpd STATIC ${KHTTPD_SOURCE_FILES} ${KHTTPD_INCLUDE_FILES})
target_include_directories(khttpd PUBLIC ${KHTTPD_INCLUDE_DIR})
target_link_libraries(khttpd ${KHTTPD_LIBS})

add_executable(server ${CMAKE_CURRENT_SOURCE_DIR}/../example/server.cpp)
target_include_directories(server PUBLIC ${KHTTPD_INCLUDE_DIR})
target_link_libraries(server ${KHTTPD_LIBS} khttpd)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
	set(KHTTPD_INCLUDE_DIR ${KHTTPD_INCLUDE_DIR} PARENT_SCOPE)
	set(KHTTPD_LIBS ${KHTTPD_LIBS} PARENT_SCOPE)
	#message(STATUS "khttp_include_dir ${KHTTPD_INCLUDE_DIR}")
endif()

