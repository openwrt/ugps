cmake_minimum_required(VERSION 3.10)

PROJECT(ugps C)
ADD_DEFINITIONS(-Os -ggdb -Wextra -Wall -Werror --std=gnu99 -Wmissing-declarations -Wno-unused-parameter)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

SET(SOURCES main.c nmea)

IF(DEBUG)
  ADD_DEFINITIONS(-DDEBUG -g3)
ENDIF()

FIND_LIBRARY(ubus NAMES ubus)
FIND_LIBRARY(ubox NAMES ubox)
FIND_PATH(ubus_include_dir NAMES libubus.h)
FIND_PATH(ubox_include_dir NAMES libubox/usock.h)
INCLUDE_DIRECTORIES(${ubox_include_dir} ${ubus_include_dir})

ADD_EXECUTABLE(ugps ${SOURCES})

TARGET_LINK_LIBRARIES(ugps ${ubox} ${ubus} m)

INSTALL(TARGETS ugps
	RUNTIME DESTINATION sbin
)
