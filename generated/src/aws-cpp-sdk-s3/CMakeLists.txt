add_project(aws-cpp-sdk-s3 "C++ SDK for the AWS s3 service" aws-cpp-sdk-core)

file(GLOB AWS_S3_HEADERS
    "include/aws/s3/*.h"
)

file(GLOB AWS_S3_MODEL_HEADERS
    "include/aws/s3/model/*.h"
)

file(GLOB AWS_S3_INTERNAL_HEADERS
    "include/aws/s3/internal/*.h"
)

file(GLOB AWS_S3_SOURCE
    "source/*.cpp"
)

file(GLOB AWS_S3_MODEL_SOURCE
    "source/model/*.cpp"
)

file(GLOB S3_UNIFIED_HEADERS
    ${AWS_S3_HEADERS}
    ${AWS_S3_INTERNAL_HEADERS}
    ${AWS_S3_MODEL_HEADERS}
)

file(GLOB S3_UNITY_SRC
    ${AWS_S3_SOURCE}
    ${AWS_S3_MODEL_SOURCE}
)

if(ENABLE_UNITY_BUILD)
    enable_unity_build("S3" S3_UNITY_SRC)
endif()

file(GLOB S3_SRC
    ${S3_UNIFIED_HEADERS}
    ${S3_UNITY_SRC}
)

if(WIN32)
    #if we are compiling for visual studio, create a sane directory tree.
    if(MSVC)
        source_group("Header Files\\aws\\s3" FILES ${AWS_S3_HEADERS})
        source_group("Header Files\\aws\\s3\\internal" FILES ${AWS_S3_INTERNAL_HEADERS})
        source_group("Header Files\\aws\\s3\\model" FILES ${AWS_S3_MODEL_HEADERS})
        source_group("Source Files" FILES ${AWS_S3_SOURCE})
        source_group("Source Files\\model" FILES ${AWS_S3_MODEL_SOURCE})
    endif(MSVC)
endif()

set(S3_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/"
)

add_library(${PROJECT_NAME} ${S3_SRC})
add_library(AWS::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

set_compiler_flags(${PROJECT_NAME})
set_compiler_warnings(${PROJECT_NAME})

if(USE_WINDOWS_DLL_SEMANTICS AND BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE "AWS_S3_EXPORTS")
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_link_libraries(${PROJECT_NAME} PRIVATE ${PLATFORM_DEP_LIBS} ${PROJECT_LIBS})


setup_install()

install (FILES ${AWS_S3_HEADERS} DESTINATION ${INCLUDE_DIRECTORY}/aws/s3)
install (FILES ${AWS_S3_MODEL_HEADERS} DESTINATION ${INCLUDE_DIRECTORY}/aws/s3/model)

do_packaging()



set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME cloudian-aws-cpp-sdk-s3)

set(_pc_file "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc")
if(EXISTS "${_pc_file}")
    file(READ "${_pc_file}" _pc_contents)
    string(REPLACE "-l${PROJECT_NAME}" "-lcloudian-${PROJECT_NAME}" _pc_contents "${_pc_contents}")
    file(WRITE "${_pc_file}" "${_pc_contents}")
endif()

find_package(CUDAToolkit 13.1 REQUIRED)
find_path(NVIDIA_CUOBJ_INCLUDE_DIR
    NAMES cuobjclient.h
    HINTS ${CUDAToolkit_INCLUDE_DIRS}
    DOC "Directory containing NVIDIA's cuobjclient.h (from GPUDirect Storage)"
    REQUIRED
)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
target_include_directories(${PROJECT_NAME} PRIVATE ${NVIDIA_CUOBJ_INCLUDE_DIR})

option(S3_CLIENT_DISABLE_TRANSPARENT_RDMA "Disable transparent RDMA in GetObject/PutObject/UploadPart; always use TCP" OFF)
if(S3_CLIENT_DISABLE_TRANSPARENT_RDMA)
    # PUBLIC, not PRIVATE: IsTransparentRDMADisabled() is a constexpr
    # inline function in the public S3ClientRDMA.h header, so this
    # define must reach any consumer that includes it too - otherwise
    # their own inline copy of that function silently disagrees with
    # how this library itself was actually built.
    target_compile_definitions(${PROJECT_NAME} PUBLIC S3_CLIENT_DISABLE_TRANSPARENT_RDMA)
endif()
