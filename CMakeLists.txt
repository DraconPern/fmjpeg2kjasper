cmake_minimum_required(VERSION 2.4)

project(fmjp2k)

set (fmjp2k_VERSION_MAJOR 1)
set (fmjp2k_VERSION_MINOR 0)
set (fmjp2k_VERSION_BUILD 2)
set(fmjp2k_VERSION "${fmjp2k_VERSION_MAJOR}.${fmjp2k_VERSION_MINOR}.${fmjp2k_VERSION_BUILD}")

option(BUILD_SHARED_LIBS "Build fmjp2k shared library and link executables against it." ON)

find_package(DCMTK REQUIRED CONFIG HINTS ${DCMTK_DIR})

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR} ${DCMTK_INCLUDE_DIRS} ${JASPER}/include)

add_definitions(-DOPJ_STATIC)
add_definitions(-Dfmjp2k_EXPORTS)

set(fmjp2k_HEADERS
	fmjp2kcd.h
	fmjp2kcp.h
	fmjp2kcrg.h
)

set(fmjp2k_SRCS
    ${fmjp2k_HEADERS}
	fmjp2kcd.cpp
	fmjp2kcp.cpp
	fmjp2kcrg.cpp
)

if(WIN32)
	add_definitions(-D_BIND_TO_CURRENT_VCLIBS_VERSION=1)
	add_definitions(-DJAS_WIN_MSVC_BUILD)
endif()

#JASPER stuff
add_definitions(-DEXCLUDE_MIF_SUPPORT)
add_definitions(-DEXCLUDE_PNM_SUPPORT)
add_definitions(-DEXCLUDE_BMP_SUPPORT)
add_definitions(-DEXCLUDE_RAS_SUPPORT)
add_definitions(-DEXCLUDE_JPG_SUPPORT)
add_definitions(-DEXCLUDE_PGX_SUPPORT)

add_library(fmjp2k ${fmjp2k_SRCS})
set(fmjp2k_LIBRARY_NAME fmjp2k)
TARGET_LINK_LIBRARIES(fmjp2k
	${DCMTK_LIBRARIES}

	)

include(GenerateExportHeader)
generate_export_header(fmjp2k)
set_property(TARGET fmjp2k PROPERTY VERSION ${fmjp2k_VERSION})
set_property(TARGET fmjp2k PROPERTY SOVERSION 1)
set_property(TARGET fmjp2k PROPERTY
  INTERFACE_fmjp2k_MAJOR_VERSION 1)
set_property(TARGET fmjp2k APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING fmjp2k_MAJOR_VERSION
)

install(TARGETS fmjp2k EXPORT fmjp2kTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(
  FILES
    ${fmjp2k_HEADERS}
  DESTINATION
    include/fmjp2k
  COMPONENT
    Devel
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/fmjp2k/fmjp2kConfigVersion.cmake"
  VERSION ${fmjp2k_VERSION}
  COMPATIBILITY AnyNewerVersion
)

export(EXPORT fmjp2kTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/fmjp2k/fmjp2kTargets.cmake"
)
configure_file( ${CMAKE_SOURCE_DIR}/cmake/fmjp2kConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/fmjp2kConfig.cmake
  @ONLY
)
#configure_file(cmake/fmjp2kConfig.cmake
#  "${CMAKE_CURRENT_BINARY_DIR}/fmjp2k/fmjp2kConfig.cmake"
#  COPYONLY
#)

set(ConfigPackageLocation lib/cmake/fmjp2k)
install(EXPORT fmjp2kTargets
  FILE
    fmjp2kTargets.cmake
  DESTINATION
    ${ConfigPackageLocation}
)


install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/fmjp2kConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/fmjp2k/fmjp2kConfigVersion.cmake"
  DESTINATION
    ${ConfigPackageLocation}
  COMPONENT
    Devel
)
