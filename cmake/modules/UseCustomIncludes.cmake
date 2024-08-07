#
# - Include a custom CMake file relative to the current source directory.
# - If no filename is provided, CMakeListsCustom.txt is used.
#
macro( ADD_CUSTOM_CMAKE_INCLUDE  )
    if( ${ARGC} GREATER 0 )
        set( _file_list ${ARGN} )
    else()
        set( _file_list CMakeListsCustom.txt )
    endif()
    foreach (_include ${_file_list})
        set( _include_file ${CMAKE_CURRENT_SOURCE_DIR}/${_include} )
        if( EXISTS ${_include_file} )
            message ( STATUS "Custom file found, including: ${_include_file}" )
            include( ${_include_file} )
        endif()
    endforeach()
endmacro()
