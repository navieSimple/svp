set(CMAKE_C_FLAGS "-fPIC")
add_library(lz4encoder SHARED lz4_encoder.c)
target_link_libraries(lz4encoder lz4)
install(TARGETS lz4encoder DESTINATION /usr/lib/svp/codec)
