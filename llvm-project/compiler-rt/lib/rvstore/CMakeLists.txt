message(STATUS "__VISIT__: RVSTORE CMakeLists file.")

include_directories(..)

# Runtime library sources.
set(rvstore_RTL_SOURCES
  rvstore.cc
  )

set(rvstore_RTL_HEADERS
  rvstore.h
  )

# build flags
set(RVSTORE_RTL_CFLAGS ${SANITIZER_COMMON_CFLAGS} " -fpermissive -O3")
append_rtti_flag(OFF RVSTORE_RTL_CFLAGS)

# Static runtime library.
add_compiler_rt_component(rvstore)

# set(RVSTORE_SUPPORTED_ARCH x86_64)

message(STATUS "SANITIZER_COMMON_SUPPORTED_OS: ${SANITIZER_COMMON_SUPPORTED_OS}")
message(STATUS "RVSTORE_SUPPORTED_ARCH: ${RVSTORE_SUPPORTED_ARCH}")
message(STATUS "rvstore_RTL_SOURCES: ${rvstore_RTL_SOURCES}")
message(STATUS "rvstore_RTL_HEADERS: ${rvstore_RTL_HEADERS}")
message(STATUS "RVSTORE_RTL_CFLAGS: ${RVSTORE_RTL_CFLAGS}")


add_compiler_rt_object_libraries(RTrvstore
    OS ${SANITIZER_COMMON_SUPPORTED_OS}
    ARCHS ${RVSTORE_SUPPORTED_ARCH}
    SOURCES ${rvstore_RTL_SOURCES}
    ADDITIONAL_HEADERS ${rvstore_RTL_HEADERS}
    CFLAGS ${RVSTORE_RTL_CFLAGS})

foreach(arch ${RVSTORE_SUPPORTED_ARCH})
  add_compiler_rt_runtime(clang_rt.rvstore
    SHARED
    ARCHS ${arch}
    OBJECT_LIBS RTrvstore
    CFLAGS ${RVSTORE_RTL_CFLAGS}
    PARENT_TARGET rvstore)
    # TODO: maybe for sybols
    # add_dependencies(rvstore
    #   clang_rt.rvstore-${arch}-symbols)
endforeach()
