if(NOT "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
endif()
# add_subdirectory(unpack)
# add_subdirectory(args)
# add_subdirectory(branch_miss)
# add_subdirectory(inter_branch)
# add_subdirectory(stack_hist)

if(NOT WIN32)
  add_subdirectory(usb)
endif()
add_subdirectory(stdio)
