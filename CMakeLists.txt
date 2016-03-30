cmake_minimum_required(VERSION 2.4)

project(fmjpeg2k)

set(VERSION "1.0.1")

SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")

find_package(dcmtk REQUIRED CONFIG HINTS ${DCMTK_DIR})

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR} ${DCMTK_INCLUDE_DIRS} ${JASPER}/src/libjasper/include )
LINK_DIRECTORIES(${ZLIB_LIBDIR})

set(fmjpeg2k_HEADERS
	fmjpeg2kcd.h
	fmjpeg2kcp.h
	fmjpeg2kcrg.h	
)

set(fmjpeg2k_SRCS
    ${fmjpeg2k_HEADERS}    
	fmjpeg2kcd.cpp
	fmjpeg2kcp.cpp
	fmjpeg2kcrg.cpp	
)


#VC++ stuff
add_definitions(-D_BIND_TO_CURRENT_VCLIBS_VERSION=1)

#JASPER stuff
add_definitions(-DJAS_WIN_MSVC_BUILD)
add_definitions(-DEXCLUDE_MIF_SUPPORT)
add_definitions(-DEXCLUDE_PNM_SUPPORT)
add_definitions(-DEXCLUDE_BMP_SUPPORT)
add_definitions(-DEXCLUDE_RAS_SUPPORT)
add_definitions(-DEXCLUDE_JPG_SUPPORT)
add_definitions(-DEXCLUDE_PGX_SUPPORT)

add_library(fmjpeg2k ${fmjpeg2k_SRCS})


TARGET_LINK_LIBRARIES(fmjpeg2k 			
	libi2d
	dcmdata 
	dcmtls 
	dcmjpeg 
	dcmjpls
	charls
	dcmimgle 
	dcmimage 
	ofstd
	oflog 	 
	ijg8 
	ijg12 
	ijg16 			
	)
	
	
install(TARGETS fmjpeg2k            	  
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
      ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib	  
	  )
	 
install(FILES ${fmjpeg2k_HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/fmjpeg2k)