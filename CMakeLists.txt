cmake_minimum_required(VERSION 3.0)
project(khttpd)
file(GLOB KHTTPD_SOURCE_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB KHTTPD_INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

file(GLOB KDAVLIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/davlib/*.cpp)
file(GLOB KDAVLIB_INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/davlib/*.h)

set(USE_CXX ON)
if (NOT KASYNC_DIR) 
	message(FATAL "config -DKASYNC_DIR")
endif()
add_subdirectory(${KASYNC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/kasync EXCLUDE_FROM_ALL)


set(KHTTPD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

#message(STATUS ${KASYNC_SOURCE_FILES})
list(APPEND KHTTPD_SOURCE_FILES ${KASYNC_SOURCE_FILES})
list(APPEND KHTTPD_INCLUDE_DIR ${KASYNC_INCLUDE_DIR})
list(APPEND KHTTPD_INCLUDE_FILES ${KASYNC_INCLUDE_FILES})

set(KHTTPD_LIBS ${KASYNC_LIBS})
add_library(khttpd STATIC ${KHTTPD_SOURCE_FILES} ${KHTTPD_INCLUDE_FILES})
target_include_directories(khttpd PUBLIC ${KHTTPD_INCLUDE_DIR})
#target_link_libraries(khttpd ${KHTTPD_LIBS})

set(KWEBLIB_INCLUDE_DIR ${KHTTPD_INCLUDE_DIR})
list(APPEND KWEBLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/davlib)
add_library(davlib STATIC ${KDAVLIB_SOURCE_FILES} ${KDAVLIB_INCLUDE_FILES})
target_include_directories(davlib PUBLIC ${KWEBLIB_INCLUDE_DIR})
#target_link_libraries(davlib ${KHTTPD_LIBS} khttpd)

add_executable(server_example ${CMAKE_CURRENT_SOURCE_DIR}/example/server.cpp)
target_include_directories(server_example PUBLIC ${KHTTPD_INCLUDE_DIR})
target_link_libraries(server_example ${KHTTPD_LIBS} khttpd)

add_executable(webdav_client_example ${CMAKE_CURRENT_SOURCE_DIR}/example/webdav.cpp)
target_include_directories(webdav_client_example PUBLIC ${KWEBLIB_INCLUDE_DIR})
target_link_libraries(webdav_client_example ${KHTTPD_LIBS} khttpd davlib)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
	set(KHTTPD_INCLUDE_DIR ${KHTTPD_INCLUDE_DIR} PARENT_SCOPE)
	set(KHTTPD_LIBS ${KHTTPD_LIBS} PARENT_SCOPE)
	set(KHTTPD_SOURCE_FILES ${KHTTPD_SOURCE_FILES} PARENT_SCOPE)
	set(KHTTPD_INCLUDE_FILES ${KHTTPD_INCLUDE_FILES} PARENT_SCOPE)

	set(KDAVLIB_SOURCE_FILES ${KDAVLIB_SOURCE_FILES} PARENT_SCOPE)
	set(KDAVLIB_INCLUDE_FILES ${KDAVLIB_INCLUDE_FILES} PARENT_SCOPE)
	set(KWEBLIB_INCLUDE_DIR ${KWEBLIB_INCLUDE_DIR} PARENT_SCOPE)
endif()

