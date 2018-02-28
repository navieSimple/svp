set(SvpClient_SRCS
    main.cpp
    Channel.cpp
    ChannelManager.cpp
    ConnectDialog.cpp
    CursorChannel.cpp
    Codec.cpp
    DisplayChannel.cpp
    InputChannel.cpp
    MainChannel.cpp
    MainWindow.cpp
    SvpWidget.cpp
    SvpRecorder.cpp
    SvpRender.cpp
    InfoWidget.cpp
    SettingsDialog.cpp
    SettingsItemDelegate.cpp
    SvpSettings.cpp
    Scancode.cpp
    USBChannel.cpp
    icon.rc
)
if(WIN32)
    list(APPEND SvpClient_SRCS KeyboardThread.cpp)
endif()
set(CMAKE_CXX_FLAGS "-std=c++11")
qt5_wrap_ui(SvpClient_SRCS
    InfoWidget.ui
    ConnectDialog.ui
    SettingsDialog.ui
    MainWindow.ui
)
qt5_add_resources(SvpClient_SRCS SvpClient.qrc)
add_executable(SvpClient WIN32 ${SvpClient_SRCS})
target_link_libraries(SvpClient Qt5::Widgets Qt5::Network lz4 gdi config usb)
install(TARGETS SvpClient DESTINATION bin)
