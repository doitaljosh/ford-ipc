cmake_minimum_required(VERSION 3.20)
project(ford_adas_ge_ipc VERSION 4.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
set(CMAKE_BUILD_RPATH "$ORIGIN")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

option(FORD_ADAS_BUILD_UI "Build SDL/FreeType HMI and application" ON)
option(FORD_ADAS_STATIC_CORE "Statically link protocol/safety/CAN/core libraries" ON)
option(FORD_ADAS_NATIVE_TUNE "Tune non-safety HMI generated code for this machine" OFF)
option(FORD_ADAS_WERROR "Treat compiler warnings as errors" ON)
option(FORD_ADAS_ENABLE_SANITIZERS "Build with ASan+UBSan (development only)" OFF)

find_package(Threads REQUIRED)
if(FORD_ADAS_BUILD_UI)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
endif()

set(PROJECT_INCLUDE "${CMAKE_SOURCE_DIR}/include")
set(COMMON_WARNINGS
    -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wundef
    -Wnull-dereference -Wimplicit-fallthrough -Wconversion -Wsign-conversion
    -Wcast-align -Wcast-qual -Wswitch-enum
)
set(HARDEN_COMPILE -fstack-protector-strong -fno-common -fno-omit-frame-pointer)
set(HARDEN_LINK -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)

if(FORD_ADAS_STATIC_CORE)
    set(CORE_LIBRARY_TYPE STATIC)
else()
    set(CORE_LIBRARY_TYPE SHARED)
endif()

function(ford_harden_target target)
    target_include_directories(${target} PUBLIC "${PROJECT_INCLUDE}")
    target_compile_options(${target} PRIVATE ${COMMON_WARNINGS} ${HARDEN_COMPILE})
    target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS=1)
    if(FORD_ADAS_WERROR)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
    if(FORD_ADAS_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

function(ford_harden_link target)
    target_link_options(${target} PRIVATE ${HARDEN_LINK})
endfunction()

add_library(ford_protocol ${CORE_LIBRARY_TYPE} src/lib/ford_decoder.cpp)
ford_harden_target(ford_protocol)

add_library(cluster_safety ${CORE_LIBRARY_TYPE} src/lib/safety.cpp)
ford_harden_target(cluster_safety)
target_link_libraries(cluster_safety PUBLIC ford_protocol Threads::Threads)

add_library(ford_can ${CORE_LIBRARY_TYPE} src/lib/can_transport.cpp)
ford_harden_target(ford_can)
target_link_libraries(ford_can PUBLIC ford_protocol cluster_safety Threads::Threads)

add_library(cluster_core ${CORE_LIBRARY_TYPE} src/lib/cluster_logic.cpp src/lib/app_services.cpp)
ford_harden_target(cluster_core)
target_link_libraries(cluster_core PUBLIC ford_protocol)

if(NOT FORD_ADAS_STATIC_CORE)
    foreach(t ford_protocol cluster_safety ford_can cluster_core)
        set_target_properties(${t} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 4)
        ford_harden_link(${t})
    endforeach()
endif()

if(FORD_ADAS_BUILD_UI)
    add_library(cluster_ui STATIC src/lib/ui.cpp src/lib/chime_audio.cpp)
    target_include_directories(cluster_ui PUBLIC "${PROJECT_INCLUDE}" PRIVATE
        ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
    target_link_directories(cluster_ui PRIVATE ${SDL2_LIBRARY_DIRS} ${SDL2_IMAGE_LIBRARY_DIRS} ${SDL2_TTF_LIBRARY_DIRS})
    target_compile_options(cluster_ui PRIVATE ${COMMON_WARNINGS} ${HARDEN_COMPILE}
        ${SDL2_CFLAGS_OTHER} ${SDL2_IMAGE_CFLAGS_OTHER} ${SDL2_TTF_CFLAGS_OTHER})
    target_compile_definitions(cluster_ui PRIVATE _GLIBCXX_ASSERTIONS=1)
    if(FORD_ADAS_WERROR)
        target_compile_options(cluster_ui PRIVATE -Werror)
    endif()
    target_link_libraries(cluster_ui PUBLIC cluster_core ford_protocol
        ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES})

    add_executable(ford_adas_ipc src/app/main.cpp src/lib/app_config.cpp)
    target_include_directories(ford_adas_ipc PRIVATE "${PROJECT_INCLUDE}" ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
    target_link_directories(ford_adas_ipc PRIVATE ${SDL2_LIBRARY_DIRS} ${SDL2_IMAGE_LIBRARY_DIRS} ${SDL2_TTF_LIBRARY_DIRS})
    target_compile_options(ford_adas_ipc PRIVATE ${COMMON_WARNINGS} ${HARDEN_COMPILE}
        ${SDL2_CFLAGS_OTHER} ${SDL2_IMAGE_CFLAGS_OTHER} ${SDL2_TTF_CFLAGS_OTHER})
    target_compile_definitions(ford_adas_ipc PRIVATE _GLIBCXX_ASSERTIONS=1)
    if(FORD_ADAS_WERROR)
        target_compile_options(ford_adas_ipc PRIVATE -Werror)
    endif()
    ford_harden_link(ford_adas_ipc)
    target_link_libraries(ford_adas_ipc PRIVATE ford_can ford_protocol cluster_safety cluster_core cluster_ui
        ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES} Threads::Threads)

    add_custom_command(TARGET ford_adas_ipc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ford_adas_ipc>/assets"
        COMMENT "Copying Ford IPC assets next to executable")

    if(FORD_ADAS_NATIVE_TUNE AND NOT CMAKE_CROSSCOMPILING)
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-mtune=native" HAS_MTUNE_NATIVE)
        if(HAS_MTUNE_NATIVE)
            target_compile_options(ford_adas_ipc PRIVATE -mtune=native)
            target_compile_options(cluster_ui PRIVATE -mtune=native)
        endif()
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    foreach(t ford_protocol cluster_safety ford_can cluster_core)
        target_compile_options(${t} PRIVATE -O2)
        target_compile_definitions(${t} PRIVATE NDEBUG _FORTIFY_SOURCE=3)
    endforeach()
    if(FORD_ADAS_BUILD_UI)
        foreach(t cluster_ui ford_adas_ipc)
            target_compile_options(${t} PRIVATE -O2)
            target_compile_definitions(${t} PRIVATE NDEBUG _FORTIFY_SOURCE=3)
        endforeach()
    endif()
endif()

enable_testing()
function(add_ford_test name source)
    add_executable(${name} ${source})
    ford_harden_target(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_ford_test(decoder_test tests/decoder_test.cpp)
target_link_libraries(decoder_test PRIVATE ford_protocol)
add_ford_test(vehicle_decoder_test tests/vehicle_decoder_test.cpp)
target_link_libraries(vehicle_decoder_test PRIVATE ford_protocol)
add_ford_test(ui_decoder_test tests/ui_decoder_test.cpp)
target_link_libraries(ui_decoder_test PRIVATE ford_protocol)
add_ford_test(safety_test tests/safety_test.cpp)
target_link_libraries(safety_test PRIVATE cluster_safety ford_protocol)
add_ford_test(decoder_robustness_test tests/decoder_robustness_test.cpp)
target_link_libraries(decoder_robustness_test PRIVATE ford_protocol)
add_ford_test(cluster_logic_test tests/cluster_logic_test.cpp)
target_link_libraries(cluster_logic_test PRIVATE cluster_core ford_protocol)
add_ford_test(protocol_contract_test tests/protocol_contract_test.cpp)
target_link_libraries(protocol_contract_test PRIVATE ford_protocol)
add_ford_test(fixed_text_test tests/fixed_text_test.cpp)
target_link_libraries(fixed_text_test PRIVATE ford_protocol)

install(TARGETS ford_protocol ford_can cluster_safety cluster_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
if(FORD_ADAS_BUILD_UI)
    install(TARGETS cluster_ui ford_adas_ipc ARCHIVE DESTINATION lib RUNTIME DESTINATION bin)
endif()
install(DIRECTORY include/ DESTINATION include)
