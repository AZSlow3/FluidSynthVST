set(VST3_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/VST3_SDK" CACHE PATH "Location of VST3 SDK")
MESSAGE(STATUS "VST3_SDK_ROOT=${VST3_SDK_ROOT}")
list(APPEND CMAKE_MODULE_PATH "${VST3_SDK_ROOT}/cmake/modules")


set(ROOT "${VST3_SDK_ROOT}")
set(SDK_ROOT "${ROOT}")
set(public_sdk_SOURCE_DIR ${SDK_ROOT}/public.sdk)
set(pluginterfaces_SOURCE_DIR ${SDK_ROOT}/pluginterfaces)


include(Global)
include(AddVST3Library)
include(PlatformToolset)

include_directories(${VST3_SDK_ROOT})

set(SDK_IDE_LIBS_FOLDER FOLDER "Libraries")
set(VST_SDK TRUE)
add_subdirectory(${VST3_SDK_ROOT}/pluginterfaces)
add_subdirectory(${VST3_SDK_ROOT}/base)
add_subdirectory(${VST3_SDK_ROOT}/public.sdk)
