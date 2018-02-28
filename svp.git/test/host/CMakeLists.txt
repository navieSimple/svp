if(BUILD_SVP_SERVER)
    add_library(hostcmd_channel SHARED hostcmd_channel.c)
    target_link_libraries(hostcmd_channel zlog)
    add_definitions(-fPIC)
    add_library(nop_channel SHARED nop_channel.c)
    target_link_libraries(nop_channel zlog)
    install(TARGETS hostcmd_channel nop_channel
        DESTINATION ${SVP_PLUGIN_DIRECTORY})
endif()
