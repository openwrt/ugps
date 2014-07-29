cmake_minimum_required(VERSION 2.6)

PROJECT(ugps C)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

SET(SOURCES main.c nmea)

SET(LIBS ubox ubus)

IF(DEBUG)
  ADD_DEFINITIONS(-DDEBUG -g3)
ENDIF()

ADD_EXECUTABLE(ugps ${SOURCES})

TARGET_LINK_LIBRARIES(ugps ${LIBS})

INSTALL(TARGETS ugps
	RUNTIME DESTINATION sbin
)
