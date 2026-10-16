cmake_minimum_required(VERSION 3.14.0)

option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)

# The SDK normally puts a symbolic link to the built plug-in into the shared
# VST3 folder (C:/Users/<user>/AppData/Local/Programs/Common/VST3). Creating a
# symbolic link needs administrator rights or Windows Developer Mode, so the
# build fails with "the client does not hold the required privilege" when
# neither is available. FORCE overrides a value that an older build tree left
# in the CMake cache; pass -DSMTG_CREATE_PLUGIN_LINK=ON once the right exists.
# An explicit -DSMTG_CREATE_PLUGIN_LINK=ON is kept; anything else (empty cache
# or a stale ON inherited from an older build tree) is forced off.
if(NOT SMTG_CREATE_PLUGIN_LINK)
    set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "Create symbolic link for each Plug-in" FORCE)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

# Where the VST 3 SDK lives, in this order of preference:
#   1. -DSMTG_VST3_SDK_ROOT=<path to vst3sdk> on the cmake command line
#   2. the environment variable SMTG_VST3_SDK_ROOT
#   3. a vst3sdk folder next to this project or inside it
#   4. the path this project was developed with, kept last so that a
#      checkout elsewhere is not tied to one machine
if(NOT SMTG_VST3_SDK_ROOT)
    if(DEFINED ENV{SMTG_VST3_SDK_ROOT})
        set(SMTG_VST3_SDK_ROOT "$ENV{SMTG_VST3_SDK_ROOT}")
    else()
        foreach(candidate
                "${CMAKE_CURRENT_SOURCE_DIR}/../vst3sdk"
                "${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk"
                "${CMAKE_CURRENT_SOURCE_DIR}/../../vst-sdk_3.8.1_build-84_2026-08-11/VST_SDK/vst3sdk")
            if(EXISTS "${candidate}/CMakeLists.txt")
                get_filename_component(SMTG_VST3_SDK_ROOT "${candidate}" ABSOLUTE)
                break()
            endif()
        endforeach()
    endif()
endif()
if(NOT SMTG_VST3_SDK_ROOT)
    message(FATAL_ERROR "VST 3 SDK not found. Pass -DSMTG_VST3_SDK_ROOT=<path to vst3sdk>, or set that environment variable.")
endif()
message(STATUS "[Spot Aligner] VST 3 SDK: ${SMTG_VST3_SDK_ROOT}")
set(vst3sdk_SOURCE_DIR "${SMTG_VST3_SDK_ROOT}")

project(SpotAligner
    # This is your plug-in version number. Change it here only.
    # Version number symbols usable in C++ can be found in
    # source/version.h and ${PROJECT_BINARY_DIR}/projectversion.h.
    VERSION 1.0.0.0 
    DESCRIPTION "SpotAligner VST 3 Plug-in"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(SpotAligner
    source/version.h
    source/cids.h
    source/soundSpeed.h
    source/parameters.h
    source/processor.h
    source/processor.cpp
    source/controller.h
    source/arrayParameters.cpp
source/controller.cpp
    source/speedSourceView.cpp
    source/panoramaGeometry.h
    source/panoramaView.cpp
    source/formatRules.h
    source/formatOptionsView.cpp
    source/spotPanner.h
    source/trackStatusView.cpp
    source/entry.cpp
)

#- VSTGUI Wanted ----
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(SpotAligner
        PRIVATE
            resource/editor.uidesc
    )
    target_link_libraries(SpotAligner
        PRIVATE
            vstgui_support
    )
    smtg_target_add_plugin_resources(SpotAligner
        RESOURCES
            "resource/editor.uidesc"
    )
endif(SMTG_ENABLE_VSTGUI_SUPPORT)
# -------------------

smtg_target_add_plugin_snapshots (SpotAligner
    RESOURCES
        resource/9F97441235D9555E9035C47CCD4AECFB_snapshot.png
        resource/9F97441235D9555E9035C47CCD4AECFB_snapshot_2.0x.png
)

target_link_libraries(SpotAligner
    PRIVATE
        sdk
)

smtg_target_configure_version_file(SpotAligner)

if(SMTG_MAC)
    smtg_target_set_bundle(SpotAligner
        BUNDLE_IDENTIFIER com.Forrest.SpotAligner
        COMPANY_NAME "Forrest's Plug-ins"
    )
    smtg_target_set_debug_executable(SpotAligner
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(SpotAligner PRIVATE 
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT SpotAligner)

        smtg_target_set_debug_executable(SpotAligner
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)
