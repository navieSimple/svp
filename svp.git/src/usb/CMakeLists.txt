set(usb_SRCS
    USBDevice.cpp
    USBMonitor.cpp
    USBUtils.cpp)
add_library(usb ${usb_SRCS})
target_link_libraries(usb Qt5::Core)
if(WIN32)
    target_link_libraries(usb ${CMAKE_CURRENT_SOURCE_DIR}/win32/libusb-1.0.a)
    target_link_libraries(usb ${CMAKE_CURRENT_SOURCE_DIR}/win32/libwdi.a setupapi)
endif()
