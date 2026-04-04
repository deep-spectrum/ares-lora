#[[
  python_module(<name> <src>...
      [DESTINATION <path>]
      [DEPENDENCIES <target>...]
      [LIBS <lib>...]
      [DEFINITIONS <def>...]
      [INSTALL_LIBS <file>...]
      [PYBIND_LIBS <lib>...])
  Defines and installs a Python C++ extension module.

  Arguments:
    <name>         – Target name for the module.
    <src>          – Path to the .cpp files for pybind11.
    DESTINATION    – Install path (relative to prefix).
    DEPENDENCIES   – Other targets this depends on.
    LIBS           – Libraries to link against.
    DEFINITIONS    – Preprocessor definitions.
    INSTALL_LIBS   – Shared libraries to install with the module.
    PYBIND_LIBS    - Pybind11 libraries to link to. Defaults to pybind11::headers if not specified.

  Requires CMake >= 3.15
]]
function(python_module name)
    # Required arguments
    set(options)
    set(one_value_args DESTINATION)
    set(multi_value_args DEPENDENCIES LIBS DEFINITIONS INSTALL_LIBS PYBIND_LIBS)
    cmake_parse_arguments(MOD "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    python_add_library(${name} MODULE ${MOD_UNPARSED_ARGUMENTS} WITH_SOABI)

    if(MOD_DEPENDENCIES)
        add_dependencies(${name} ${MOD_DEPENDENCIES})
    endif()

    if (NOT MOD_PYBIND_LIBS)
        set(MOD_PYBIND_LIBS pybind11::headers)
    endif()

    target_link_libraries(${name} PRIVATE ${MOD_PYBIND_LIBS} ${MOD_LIBS})

    if(MOD_DEFINITIONS)
        target_compile_definitions(${name} PRIVATE ${MOD_DEFINITIONS})
    endif()

    if (NOT MOD_DESTINATION)
        message(FATAL_ERROR "python_module() DESTINATION is required")
    endif ()

    install(TARGETS ${name} DESTINATION ${MOD_DESTINATION})

    if(MOD_INSTALL_LIBS)
        foreach (lib ${MOD_INSTALL_LIBS})
            if (NOT EXISTS "${lib}")
                message(FATAL_ERROR "${lib} does not exist")
            endif ()
        endforeach ()

        install(FILES ${MOD_INSTALL_LIBS} DESTINATION ${MOD_DESTINATION}/lib)
        if(APPLE)
            set(origin_token "@loader_path")
        else()
            set(origin_token "$ORIGIN")
        endif()
        set_property(TARGET ${name} PROPERTY INSTALL_RPATH "${origin_token}/lib")
    endif()
endfunction()

#[[
  python_package(<package_src_dir>
      [DESTINATION <path>])
  Installs a Python package given the relative path to the package.

  Arguments:
    <package_src_dir>  - Path to the Python package to be installed.
    DESTINATION        - Install path (relative to the prefix).

  Requires CMake >= 3.15
]]
function(python_package package_src_dir)
    set(options)
    set(one_value_args DESTINATION)
    set(multi_value_args)
    cmake_parse_arguments(PKG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    get_filename_component(abs_src_dir ${package_src_dir} ABSOLUTE)
    if(NOT IS_DIRECTORY ${abs_src_dir})
        message(FATAL_ERROR "install_python_package: '${package_src_dir}' is not a valid directory")
    endif()

    if (NOT PKG_DESTINATION)
        message(FATAL_ERROR "python_package() DESTINATION is required")
    endif ()

    install(
            DIRECTORY ${abs_src_dir}/
            DESTINATION ${PKG_DESTINATION}
            FILES_MATCHING
            PATTERN "*.py"
    )
endfunction()
