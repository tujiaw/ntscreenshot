# CMake's compiler detection can cache the localized /showIncludes prefix in
# the Windows code page. Ninja decodes compiler output to UTF-8, so that prefix
# silently matches nothing and header changes leave incompatible objects behind.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    set(nt_dependency_probe "${CMAKE_BINARY_DIR}/CMakeFiles/nt-dependency-probe.cpp")
    file(WRITE "${nt_dependency_probe}" "#include <stddef.h>\n")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /EP "${nt_dependency_probe}"
        OUTPUT_VARIABLE nt_dependency_output
        ERROR_VARIABLE nt_dependency_error
        RESULT_VARIABLE nt_dependency_result
        # cl emits the localized prefix in the Windows ANSI code page. AUTO
        # varies between console and redirected builds and can preserve raw bytes.
        ENCODING ANSI)
    string(REGEX MATCH "([^\r\n]+:)[ \t]+[A-Za-z]:[/\\\\]" nt_dependency_match "${nt_dependency_output}\n${nt_dependency_error}")
    if(NOT nt_dependency_result EQUAL 0 OR NOT nt_dependency_match)
        message(FATAL_ERROR "Cannot detect MSVC header dependencies: ${nt_dependency_error}")
    endif()
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_1}")
    # Subdirectories may reload CMakeCXXCompiler.cmake when enabling languages.
    # Keep the language-specific value in sync so it cannot restore raw bytes.
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_CL_SHOWINCLUDES_PREFIX}")
    unset(nt_dependency_probe)
    unset(nt_dependency_output)
    unset(nt_dependency_error)
    unset(nt_dependency_result)
    unset(nt_dependency_match)
endif()
