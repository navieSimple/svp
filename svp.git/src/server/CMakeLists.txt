set(server_SRCS
    display_worker.c
    graphic.c
    server.c
    svp_server.c
    svp.c
    vrde.c
    channel.c
    cmd.c
    inqueue.c
    outqueue.c
    main_channel.c
    display_channel.c
    cursor_channel.c
    input_channel.c
    usb_channel.c
    codec.c
    encoder.c
)
add_definitions(-DBUILD_SVP_SERVER)
set(CMAKE_C_FLAGS "-std=gnu99 -fPIC")
add_library(svp SHARED ${server_SRCS})
target_link_libraries(svp uuid lz4 config zlog)
install(TARGETS svp DESTINATION lib)
