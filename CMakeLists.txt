cmake_minimum_required(VERSION 3.18)
project(r2k C)

set(CMAKE_C_STANDARD 90)

add_executable(r2k main.c)

set(CMAKE_C_FLAGS "-O2 -I/usr/X11R6/include -I/usr/include/freetype2 -L/usr/X11R6/lib -lXft -lX11 -lfontconfig")
        # "-D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700"
        # "-D_POSIX_C_SOURCE=200809L -DVERSION=\"0.1\""
        # )
message("CMAKE_C_FLAGS = " ${CMAKE_C_FLAGS})


#find_package(X11 REQUIRED)
#target_link_libraries(r2k ${X11_LIBRARIES})
#target_include_directories(r2k PRIVATE ${X11_INCLUDE_DIRS})
#find_package(Freetype REQUIRED)
#target_link_libraries(r2k ${FREETYPE_LIBRARIES})
#target_include_directories(r2k PRIVATE ${FREETYPE_INCLUDE_DIRS})

