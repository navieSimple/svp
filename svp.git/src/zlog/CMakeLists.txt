set(zlog_SRCS buf.c
    category.c
    category_table.c
    conf.c
    event.c
    format.c
    level.c
    level_list.c
    mdc.c
    record.c
    record_table.c
    rotater.c
    rule.c
    spec.c
    thread.c
    zc_arraylist.c
    zc_hashtable.c
    zc_profile.c
    zc_util.c
    zlog.c
)
if(UNIX)
    set(CMAKE_C_FLAGS "-fPIC")
endif()
add_library(zlog SHARED ${zlog_SRCS})
install(TARGETS zlog DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES zlog.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/zlog)
