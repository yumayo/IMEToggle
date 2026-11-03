cmake_minimum_required(VERSION 3.20)
project(IMEToggle VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# MSVC builds also ship without the Visual C++ Redistributable requirement.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(key_remapper INTERFACE)
target_include_directories(key_remapper INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/IMEToggle")

function(imetoggle_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/utf-8>")
    else()
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic;-Werror>")
    endif()
endfunction()

if(WIN32)
    enable_language(RC)
    add_executable(IMEToggle WIN32
        src/IMEToggle/main.cpp src/IMEToggle/Win32Support.cpp src/IMEToggle/IMEToggle.rc)
    target_link_libraries(IMEToggle PRIVATE key_remapper user32 shell32 advapi32 gdi32)
    target_compile_definitions(IMEToggle PRIVATE
        UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    imetoggle_warnings(IMEToggle)
    set_property(SOURCE src/IMEToggle/IMEToggle.rc APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/IMEToggle/app.manifest")
    if(MSVC)
        # The application manifest is already embedded by IMEToggle.rc.
        target_link_options(IMEToggle PRIVATE /MANIFEST:NO)
    endif()
    if(MINGW)
        target_link_options(IMEToggle PRIVATE -municode -static -static-libgcc -static-libstdc++)
        add_custom_command(TARGET IMEToggle POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DEXECUTABLE=$<TARGET_FILE:IMEToggle>"
                "-DOBJDUMP=${CMAKE_OBJDUMP}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify-runtime.cmake"
            VERBATIM)
    endif()
    install(TARGETS IMEToggle RUNTIME DESTINATION .)
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(key_remapper_tests tests/key_remapper_tests.cpp)
    target_link_libraries(key_remapper_tests PRIVATE key_remapper)
    imetoggle_warnings(key_remapper_tests)
    foreach(test_name IN ITEMS explicit-state repeats modifiers pause resume injection-failure unrelated overlap no-sender)
        add_test(NAME "key-remapper.${test_name}" COMMAND key_remapper_tests "${test_name}")
    endforeach()
endif()
