# DO NOT CHANGE THE SYMBOLIC VARIABLE `SOURCES`.
# You can, however, change the list of files that comprise this variable.

include_directories(include)
set(SOURCES src/main.cpp
            src/stubborn_link.cpp
            src/perfect_link.cpp
        src/packet.cpp
            src/process.cpp
            src/config.cpp
        src/thread_pool.cpp
        src/udp_socket.cpp
        src/event_loop.cpp)

# DO NOT EDIT THE FOLLOWING LINES
find_package(Threads)
add_executable(da_proc ${SOURCES})
target_link_libraries(da_proc ${CMAKE_THREAD_LIBS_INIT})

# Enable ThreadSanitizer for supported compilers (GCC and Clang)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Enabling ThreadSanitizer for supported compiler")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
endif()
