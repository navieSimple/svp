add_executable(tst_hostcmd tst_hostcmd.c)
target_link_libraries(tst_hostcmd guest)
add_executable(tst_speed tst_speed.c)
target_link_libraries(tst_speed guest)
