cmake_minimum_required(VERSION 3.16)
project(entry)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DOHOS_PLATFORM)

# host/fw2-hap/entry/src/main/cpp → ../../../../../.. = repo root
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)

set(WX_BUILD "${REPO_ROOT}/build-wx-ohos-gui")
set(WX_SRC_INCLUDE "${REPO_ROOT}/third_party/wxWidgets/include")
set(WX_SETUP_INCLUDE "${WX_BUILD}/lib/wx/include/ohos-unicode-3.3")
set(WX_LIB_DIR "${WX_BUILD}/lib")
set(CL_BUILD "${REPO_ROOT}/build-codelite-ohos")
set(CL_LIB_DIR "${CL_BUILD}/lib")
set(HAP_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/arm64-v8a")

set(FW2_COMPILER_DIR "${REPO_ROOT}/host/fw2-hap/compiler")

message(STATUS "Host REPO_ROOT=${REPO_ROOT}")
message(STATUS "Host WX_LIB_DIR=${WX_LIB_DIR}")
message(STATUS "Host CL_LIB_DIR=${CL_LIB_DIR}")
message(STATUS "Host FW2_COMPILER_DIR=${FW2_COMPILER_DIR}")

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${WX_SETUP_INCLUDE}
                    ${WX_SRC_INCLUDE}
                    ${FW2_COMPILER_DIR}
                    ${REPO_ROOT}/codelite/LiteEditor
                    ${REPO_ROOT}/codelite/CodeLite
                    ${REPO_ROOT}/codelite/PCH)

# Match exactly the defines used when building consumer TUs (*not* the
# wx library itself).  The prebuilt libwx_ohosu_core-3.3-OHOS.so was built
# with -DWXMAKINGDLL_CORE -DWXBUILDING -DWX_PRECOMP -DwxUSE_GUI=1
# -DwxUSE_BASE=0; but us (libentry.so = the wx *consumer*) must mirror the
# *include-side* view: -DWXUSINGDLL, -DwxUSE_GUI=1, -DwxUSE_BASE=1.
# DO NOT set -DWX_PRECOMP on the consumer side — that flag is only valid
# together with an actual CMake precompiled header (cmake_pch.hxx) emitted
# by wx's own build.  Without the matching .pch file, opening -DWX_PRECOMP
# silently shuffles the layout of inlines (wxObject::operator=,
# wxWindowBase::SetAcceleratorTable, …) and causes TabgroupsPane to SIGSEGV.
#
# wx/mdi.h dispatches to a per-port header and has no __WXOHOS__ branch, so
# force the generic-mdi fallback via wxUSE_GENERIC_MDI_AS_NATIVE=1.
add_definitions(-D__WXOHOS__
                -DWXUSINGDLL
                -D_FILE_OFFSET_BITS=64
                -DwxUSE_GUI=1
                -DwxUSE_BASE=1
                -DwxUSE_GENERIC_MDI_AS_NATIVE=1
                -DwxUSE_XRC=1)

add_library(entry SHARED
    napi_init.cpp
    fw2_wx_host.cpp
    fw2_xcomponent.cpp
    # Real input dispatch (4000+ lines): I-2 pointer parsing, I-3 wxMouseEvent
    # build, I-4 hit target resolve, I-5 MenuBar hit+popup,
    # FlushPendingMenuCommand → frame->ProcessCommand.
    # NOTE: every wx inline out-of-line copy this TU emits is still fully
    # covered by the --version-script ODR fix (see target_link_options
    # below) so TabgroupsPane::SetAcceleratorTable still resolves to the
    # correct producer-side layout inside libwx_ohosu_core.so.
    fw2_input.cpp
    fw2_present_probe.cpp
    fw2_process_probe.cpp
    fw2_toolchain_probe.cpp
    fw2_process_output_probe.cpp
    fw2_compiler_probe.cpp
    fw2_runner_probe.cpp
    fw2_build_probe.cpp
    fw2_build_bridge.cpp
    fw2_project_bridge.cpp
    fw2_project_dialog.cpp
    fw2_project_probe.cpp
    ${FW2_COMPILER_DIR}/compiler_run_util.cpp
    ${FW2_COMPILER_DIR}/project_config.cpp
    ${FW2_COMPILER_DIR}/local_compiler_backend.cpp
    ${FW2_COMPILER_DIR}/remote_compiler_backend.cpp
    ${FW2_COMPILER_DIR}/runner_backend.cpp
    ${FW2_COMPILER_DIR}/build_controller.cpp
    fw2_filepicker.cpp
    f4_paths_shim.cpp
    fw2_ohos_tlw_link_stubs.cpp)

target_link_directories(entry PUBLIC ${WX_LIB_DIR} ${CL_LIB_DIR} ${HAP_LIBS_DIR})

# The critical ODR / weak-symbol fix: use the GNU linker's version script
# to trim libentry.so's dynamic-symbol table down to ONLY the cross-module
# bridge entry points (Fw2_* + clStandardPaths_* + the NAPI constructor).
# Everything else — and in particular every weak out-of-line copy of a
# wxWidgets inline method (wxObject::operator=, wxAcceleratorTable::operator=,
# wxWindowBase::SetAcceleratorTable, …) that clang emits into every TU that
# includes wx/window.h — is demoted from STB_GLOBAL to LOCAL in the final
# .dynsym by the `local: *;` wildcard at the bottom.
#
# Without this script, ARM64 dynsym resolution picks libentry.so's inline
# copy (consumer side layout) over the one compiled into
# libwx_ohosu_core-3.3-OHOS.so (producer side, built with WXBUILDING + cmake
# PCH + different inline expansion).  That layout mismatch is exactly what
# TabgroupsPane::SetAcceleratorTable hits when it calls wxObject::Ref with
# the wrong ref-count field offset → SIGSEGV at +144.
#
# We intentionally do NOT use -fvisibility=hidden on the compiler side: if
# we did, we would have to mark every Fw2_* function STB_GLOBAL via source
# annotations, and any forgotten one would silently stop being visible to
# dlsym() from libcodelite_app.so.  The linker script is purely additive
# from the source author's point of view — adding a new bridge entry point
# requires only one line here (plus matching prototype in the header).
set(FW2_DYNLIST "${CMAKE_CURRENT_BINARY_DIR}/libentry_dynamic_list.txt")
file(WRITE "${FW2_DYNLIST}" [=[
{
  global:
    Fw2_*;
    _Z*Fw2*;
    clStandardPaths_*;
    _Z*clStandardPaths*;
    RegisterEntryModule;
    _Z*RegisterEntryModule*;
    f4_*;
    _Z*f4_*;
  local:
    *;
};
]=])
target_link_options(entry PRIVATE
    "-Wl,--version-script,${FW2_DYNLIST}")

# Prefer staged HAP libs (arm64-v8a) for packaging; build dirs for link.
set(_CL_APP_CANDIDATES
    "${HAP_LIBS_DIR}/libcodelite_app.so"
    "${CL_LIB_DIR}/libcodelite_app.so"
    "${CL_BUILD}/LiteEditor/libcodelite_app.so")
set(_CL_APP_LIB "")
foreach(_c ${_CL_APP_CANDIDATES})
    if(EXISTS "${_c}")
        set(_CL_APP_LIB "${_c}")
        break()
    endif()
endforeach()

target_link_libraries(entry PUBLIC
    libace_napi.z.so
    libace_ndk.z.so
    libhilog_ndk.z.so
    libnative_window.so
    libnative_buffer.so
    libEGL.so
    libGLESv2.so
    wx_ohosu_core-3.3-OHOS
    wx_ohosu_aui-3.3-OHOS
    wx_ohosu_stc-3.3-OHOS
    wx_ohosu_xrc-3.3-OHOS
    wx_baseu-3.3-OHOS
    z
    c++_shared)

if(_CL_APP_LIB)
    message(STATUS "Host: linking official CodeLiteApp: ${_CL_APP_LIB}")
    target_link_libraries(entry PUBLIC "${_CL_APP_LIB}")
else()
    message(WARNING "Host: libcodelite_app.so not found — build target codelite_app first. "
                    "EmbeddedStart will be weak-null until then (Minimal Boot blocked at B-1).")
endif()
