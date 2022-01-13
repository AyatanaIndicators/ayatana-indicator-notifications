set (SERVICE_LIB "ayatanaindicatornotificationsservice")
set (SERVICE_EXEC "ayatana-indicator-notifications-service")
add_definitions(-DG_LOG_DOMAIN="ayatana-indicator-notifications")

# handwritten sources
set(SERVICE_MANUAL_SOURCES
    urlregex.c
    notification.c
    dbus-spy.c
    service.c)

# add the bin dir to our include path so the code can find the generated header files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# the service library for tests to link against (basically, everything except main())
add_library(${SERVICE_LIB} STATIC ${SERVICE_MANUAL_SOURCES})
include_directories(${CMAKE_SOURCE_DIR})
link_directories(${SERVICE_DEPS_LIBRARY_DIRS})

# the executable: lib + main()
add_executable (${SERVICE_EXEC} main.c)
target_link_libraries (${SERVICE_EXEC} ${SERVICE_LIB} ${SERVICE_DEPS_LIBRARIES})
install (TARGETS ${SERVICE_EXEC} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR})
