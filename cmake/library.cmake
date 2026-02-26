#=============================================================================
# function: resolve_target_dependencies
#
# Normalizes a list of library names, resolving them to existing CMake targets.
# This is useful when dealing with a mix of target naming conventions.
# For each item in the input list, it checks in order:
#   1. If the item itself is a known CMake target, it is kept.
#   2. If not, it checks if a target named "<item>.lib" exists. If so,
#      the item is replaced with "<item>.lib".
#   3. Otherwise, the item is kept as is (as a fallback for system libraries
#      or other linker flags).
#
# USAGE:
#   resolve_target_dependencies( <name_of_output_list_variable> )
#
# PARAMETERS:
#  The name of the variable containing the list of libraries to resolve.
#
function(resolve_target_dependencies)

    # --- Main logic ---
    set(link_libs "") # Initialize an empty list for the results

    # Loop through each item in the input list
    foreach (item IN LISTS ${ARGN})
        # Check what exists at configure time first
        if (TARGET ${item})
            # Target already exists, use directly
            list(APPEND link_libs ${item})
        elseif (item MATCHES "\\.lib$")
            # Already has .lib suffix, use as-is
            list(APPEND link_libs ${item})
        elseif (TARGET ${item}.lib)
            # Target with .lib suffix exists at configure time
            list(APPEND link_libs ${item}.lib)
        else ()
            # Use generator expression for targets that don't exist yet
            list(APPEND link_libs
                    "$<IF:$<TARGET_EXISTS:${item}>,${item},$<IF:$<TARGET_EXISTS:${item}.lib>,${item}.lib,${item}>>")
        endif ()
    endforeach ()

    # Set the output variable in the parent scope so the caller can see it
    set(${ARGN} ${link_libs} PARENT_SCOPE)

endfunction()

function(create_library NAME)
    cmake_parse_arguments(
            PARSED_ARGS
            "INTERFACE;SHARED" # list of names of the boolean arguments (only defined ones will be true)
            "" # list of names of mono-valued arguments
            "RSRCS;SRCS;DEPS;P_DEPS;INCLUDES;P_INCLUDES" # list of names of multi-valued arguments (output variables are lists)
            ${ARGN}
    )

    if (NOT PARSED_ARGS_SRCS AND NOT PARSED_ARGS_INTERFACE)
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.hxx")
            set(PARSED_ARGS_SRCS ${PARSED_ARGS_SRCS} ${NAME}.hxx)
        endif ()
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.ixx")
            set(PARSED_ARGS_SRCS ${PARSED_ARGS_SRCS} ${NAME}.ixx)
        endif ()
        if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.cxx")
            set(PARSED_ARGS_SRCS ${PARSED_ARGS_SRCS} ${NAME}.cxx)
        endif ()
    endif ()

    if (PARSED_ARGS_RSRCS)
        file(GLOB_RECURSE PARSED_ARGS_SRCS CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" ${PARSED_ARGS_RSRCS})
    endif ()

    resolve_target_dependencies(PARSED_ARGS_DEPS)
    resolve_target_dependencies(PARSED_ARGS_P_DEPS)

    if (PARSED_ARGS_INTERFACE)
        add_library(${NAME}.lib INTERFACE ${PARSED_ARGS_SRCS})
        message(STATUS "Creating INTERFACE library ${NAME}")

        list(APPEND CMAKE_MESSAGE_INDENT "  ")
        message(STATUS "with ${PARSED_ARGS_SRCS}")
        list(POP_BACK CMAKE_MESSAGE_INDENT)

        set(NAME ${NAME}.lib)
        target_link_libraries(${NAME} INTERFACE ${PARSED_ARGS_DEPS})
        target_link_libraries(${NAME} INTERFACE ${PARSED_ARGS_P_DEPS})

    else ()
        if(PARSED_ARGS_SHARED)
            add_library(${NAME} SHARED ${PARSED_ARGS_SRCS})
            message(STATUS "Creating SHARED library ${NAME}")
        else()
            add_library(${NAME}.lib STATIC ${PARSED_ARGS_SRCS})
            message(STATUS "Creating STATIC library ${NAME}")
            set(NAME ${NAME}.lib)
        endif()

        list(APPEND CMAKE_MESSAGE_INDENT "  ")
        message(STATUS "with ${PARSED_ARGS_SRCS}")
        list(POP_BACK CMAKE_MESSAGE_INDENT)

        target_link_libraries(${NAME} PUBLIC ${PARSED_ARGS_DEPS})
        target_link_libraries(${NAME} PRIVATE ${PARSED_ARGS_P_DEPS})

    endif (PARSED_ARGS_INTERFACE)

    target_include_directories(${NAME} PUBLIC ${PARSED_ARGS_INCLUDES})
    target_include_directories(${NAME} PRIVATE ${PARSED_ARGS_P_INCLUDES})

    set_target_properties(${NAME} PROPERTIES LINKER_LANGUAGE CXX)

endfunction()
