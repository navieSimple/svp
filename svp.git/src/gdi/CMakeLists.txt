set(gdi_SRCS
    GDITypes.cpp
    GDIUtils.cpp
)
set(CMAKE_CXX_FLAGS "-std=c++11")
add_library(gdi STATIC ${gdi_SRCS})
target_link_libraries(gdi Qt5::Widgets)
