cmake_minimum_required ( VERSION 3.1 FATAL_ERROR )

set ( CCTZ_LIBDIR "${MANTICORE_BINARY_DIR}/cctz" )
set ( CCTZ_SRC "${MANTICORE_BINARY_DIR}/cctz-src" )
mark_as_advanced ( CCTZ_SRC CCTZ_LIBDIR )

include ( FetchContent )
# check whether we have local copy (to not disturb network)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set ( _stored_BUILD_TESTING ${BUILD_TESTING} )
set ( _stored_BUILD_EXAMPLES ${BUILD_EXAMPLES} )
set ( _stored_BUILD_TOOLS ${BUILD_TOOLS} )

set ( CCTZ_URL_GITHUB "https://github.com/manticoresoftware/cctz/archive/refs/heads/master.zip" )
set ( BUILD_TESTING OFF CACHE BOOL "" FORCE )
set ( BUILD_EXAMPLES OFF CACHE BOOL "" FORCE )
set ( BUILD_TOOLS OFF CACHE BOOL "" FORCE )
message ( STATUS "Use cctz from github ${CCTZ_URL_GITHUB}" )
FetchContent_Declare ( cctz
		SOURCE_DIR "${CCTZ_SRC}"
		BINARY_DIR "${CCTZ_LIBDIR}"
		URL ${CCTZ_URL_GITHUB}
		GIT_TAG cmake-3.x-5.7
		GIT_SHALLOW TRUE
		)

FetchContent_GetProperties ( cctz )
if ( NOT cctz_POPULATED )
	FetchContent_Populate ( cctz )
	add_subdirectory ( ${cctz_SOURCE_DIR} ${cctz_BINARY_DIR} )
endif ()

set ( BUILD_TESTING ${_stored_BUILD_TESTING} CACHE BOOL "" FORCE )
set ( BUILD_EXAMPLES ${_stored_BUILD_EXAMPLES} CACHE BOOL "" FORCE )
set ( BUILD_TOOLS ${_stored_BUILD_TOOLS} CACHE BOOL "" FORCE )

include_directories ( "${cctz_SOURCE_DIR}/include" )
target_link_libraries ( lextra INTERFACE cctz )

mark_as_advanced ( FETCHCONTENT_FULLY_DISCONNECTED FETCHCONTENT_QUIET FETCHCONTENT_UPDATES_DISCONNECTED FETCHCONTENT_SOURCE_DIR_CCTZ FETCHCONTENT_UPDATES_DISCONNECTED_CCTZ )
