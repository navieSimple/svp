include_directories(${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src)
if(WIN32)
    add_subdirectory(guest)
endif()
if(UNIX)
    add_subdirectory(host)
endif()
if(BUILD_SVP_SERVER)
    add_subdirectory(gtest)
    add_subdirectory(server)
endif()

