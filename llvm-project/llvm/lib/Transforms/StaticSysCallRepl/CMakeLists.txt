if( NOT LLVM_REQUIRES_RTTI )
  if( NOT LLVM_REQUIRES_EH )
    set(LLVM_EXPORTED_SYMBOL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/SysCallReplacer.exports)
  endif()
endif()

if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

include(FetchContent)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.10.2)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_llvm_library( LLVMStaticSysCallRepl MODULE BUILDTREE_ONLY
  SysCall.cpp
  SysCallReplacer.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
  )

target_link_libraries(LLVMStaticSysCallRepl PRIVATE nlohmann_json::nlohmann_json)