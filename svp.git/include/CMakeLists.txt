if(BUILD_SVP_SERVER)
    install(FILES svp.h vrde.h cmd.h
                list.h inqueue.h queue.h queuedef.h vrde.h cmd.h
                host_channel.h guest_channel.h
            DESTINATION include/svp)
endif()
