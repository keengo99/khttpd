cmake_minimum_required(VERSION 3.12)
project(khttpd)

option(HTTP_PROXY "build for http proxy" OFF)
option(ENABLE_HTTP2 "build for http2 enable" ON)

file(GLOB KHTTPD_SOURCE_FILES  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB KHTTPD_INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

file(GLOB KDAVLIB_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/davlib/*.cpp)
file(GLOB KDAVLIB_INCLUDE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/davlib/*.h)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

#set(USE_CXX ON)
if (NOT KASYNC_DIR) 
	message(FATAL "config -DKASYNC_DIR")
endif()
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
	#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
	#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
endif()
############## zlib ##################
if (ZLIB_DIR)
	get_filename_component(ZLIB_DIR ${ZLIB_DIR} REALPATH)
	add_subdirectory(${ZLIB_DIR} ${CMAKE_CURRENT_BINARY_DIR}/zlib EXCLUDE_FROM_ALL)
	list(APPEND KHTTPD_LIBS zlibstatic)
else()
	include(FindZLIB)
	if (NOT ZLIB_FOUND)
		message(FATAL_ERROR "cann't found zlib")
	endif()
	list(APPEND KHTTPD_LIBS ${ZLIB_LIBRARIES})
endif()

################ boringssl ################
if (BORINGSSL_DIR)
	get_filename_component(BORINGSSL_DIR ${BORINGSSL_DIR} REALPATH)	
	if (CMAKE_SYSTEM_NAME MATCHES "Windows")
	#	INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/thirdinclude)
	#	LINK_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/win32lib/${CMAKE_VS_PLATFORM_NAME}")
	endif()
	add_subdirectory(${BORINGSSL_DIR} ${CMAKE_CURRENT_BINARY_DIR}/boringssl EXCLUDE_FROM_ALL)
	set(OPENSSL_DIR ${BORINGSSL_DIR})
	if (LSQUIC_DIR)
		set(ENABLE_HTTP3 ON)
		get_filename_component(LSQUIC_DIR ${LSQUIC_DIR} REALPATH)
		set(LSQUIC_TESTS OFF)
		set(BORINGSSL_INCLUDE ${BORINGSSL_DIR}/include)
		set(BORINGSSL_LIB_ssl ssl)
		set(BORINGSSL_LIB_crypto crypto)
		add_subdirectory(${LSQUIC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/lsquic EXCLUDE_FROM_ALL)
		list(APPEND KHTTPD_LIBS lsquic)
		list(APPEND KHTTPD_INCLUDE_DIR ${LSQUIC_DIR}/include ${LSQUIC_DIR}/wincompat)		
	endif()
	install(TARGETS ssl crypto
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION bin
     )
endif()

add_subdirectory(${KASYNC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/kasync EXCLUDE_FROM_ALL)
set_source_files_properties(${KASYNC_SOURCE_FILES} PROPERTIES LANGUAGE CXX)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_compile_options(-Wno-deprecated)
endif()
list(APPEND KHTTPD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)


#message(STATUS ${KASYNC_SOURCE_FILES})
list(APPEND KHTTPD_SOURCE_FILES ${KASYNC_SOURCE_FILES})
list(APPEND KHTTPD_INCLUDE_DIR ${KASYNC_INCLUDE_DIR})
list(APPEND KHTTPD_INCLUDE_FILES ${KASYNC_INCLUDE_FILES})

list(APPEND KHTTPD_LIBS ${KASYNC_LIBS})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/khttp.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/khttp.h)

message(STATUS "KHTTP_LIBS ${KHTTPD_LIBS}")
add_library(khttpd STATIC ${KHTTPD_SOURCE_FILES} ${KHTTPD_INCLUDE_FILES})
set_target_properties(khttpd PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(khttpd PUBLIC ${KHTTPD_INCLUDE_DIR})
target_link_libraries(khttpd ${KHTTPD_LIBS})
set_target_properties(khttpd PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (NOT (CMAKE_SYSTEM_NAME MATCHES "Windows"))
	target_compile_options(khttpd PUBLIC -Wno-invalid-offsetof)
endif()


set(KDAVLIB_INCLUDE_DIR ${KHTTPD_INCLUDE_DIR})
list(APPEND KDAVLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/davlib)
add_library(davlib STATIC ${KDAVLIB_SOURCE_FILES} ${KDAVLIB_INCLUDE_FILES})
target_include_directories(davlib PUBLIC ${KDAVLIB_INCLUDE_DIR})
target_link_libraries(davlib khttpd)

add_executable(server_example ${CMAKE_CURRENT_SOURCE_DIR}/example/server.cpp)
target_include_directories(server_example PUBLIC ${KHTTPD_INCLUDE_DIR})
target_link_libraries(server_example khttpd)

add_executable(webdav_client_example ${CMAKE_CURRENT_SOURCE_DIR}/example/webdav.cpp)
target_include_directories(webdav_client_example PUBLIC ${KDAVLIB_INCLUDE_DIR})
target_link_libraries(webdav_client_example khttpd davlib)

get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
	set(KHTTPD_INCLUDE_DIR ${KHTTPD_INCLUDE_DIR} PARENT_SCOPE)
	set(KHTTPD_LIBS ${KHTTPD_LIBS} PARENT_SCOPE)
	set(KHTTPD_SOURCE_FILES ${KHTTPD_SOURCE_FILES} PARENT_SCOPE)
	set(KHTTPD_INCLUDE_FILES ${KHTTPD_INCLUDE_FILES} PARENT_SCOPE)

	set(KDAVLIB_SOURCE_FILES ${KDAVLIB_SOURCE_FILES} PARENT_SCOPE)
	set(KDAVLIB_INCLUDE_FILES ${KDAVLIB_INCLUDE_FILES} PARENT_SCOPE)
	set(KDAVLIB_INCLUDE_DIR ${KDAVLIB_INCLUDE_DIR} PARENT_SCOPE)
endif()

