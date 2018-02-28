set(guest_SRCS
    guest_channel.c
)
add_definitions(-DWIN32)
add_library(guest SHARED ${guest_SRCS})
