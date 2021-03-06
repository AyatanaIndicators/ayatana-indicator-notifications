set (SERVICE_LIB "ayatanaindicatornotificationsservice")
set (SERVICE_EXEC "ayatana-indicator-notifications-service")

add_definitions(-DG_LOG_DOMAIN="ayatana-indicator-notifications")

# handwritten sources
set(SERVICE_MANUAL_SOURCES
    urlregex.c
    notification.c
    dbus-spy.c
    service.c)

# generated sources
include(GdbusCodegen)
set(SERVICE_GENERATED_SOURCES)

# add the bin dir to our include path so the code can find the generated header files
include_directories(${CMAKE_CURRENT_BINARY_DIR})


# add warnings/coverage info on handwritten files
# but not the autogenerated ones...
set(C_WARNING_ARGS "${C_WARNING_ARGS} -Wno-bad-function-cast") # g_clear_object()
set(C_WARNING_ARGS "${C_WARNING_ARGS} -Wno-switch-enum")
set_source_files_properties(${SERVICE_MANUAL_SOURCES}
                            PROPERTIES COMPILE_FLAGS "${C_WARNING_ARGS} -g -std=c99")

# the service library for tests to link against (basically, everything except main())
add_library(${SERVICE_LIB} STATIC ${SERVICE_MANUAL_SOURCES} ${SERVICE_GENERATED_SOURCES})
include_directories(${CMAKE_SOURCE_DIR})
link_directories(${SERVICE_DEPS_LIBRARY_DIRS})

# the executable: lib + main()
add_executable (${SERVICE_EXEC} main.c)
set_source_files_properties(${SERVICE_SOURCES} main.c PROPERTIES COMPILE_FLAGS "${C_WARNING_ARGS} -std=c99")
target_link_libraries (${SERVICE_EXEC} ${SERVICE_LIB} ${SERVICE_DEPS_LIBRARIES})
install (TARGETS ${SERVICE_EXEC} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_PKGLIBEXECDIR})
