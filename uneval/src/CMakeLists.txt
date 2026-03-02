set(SOURCES
  UnevaluatedAliasToLambdaCheck.cpp
  UnevalModule.cpp
)

add_library(uneval MODULE ${SOURCES})

target_include_directories(uneval
  PRIVATE
    /usr/include/clang
    /usr/include/llvm
)

target_link_libraries(uneval
  PRIVATE
    ${CLANG_LIBS}
    ${LLVM_LIBS}
)

set_target_properties(uneval PROPERTIES
  PREFIX ""
  OUTPUT_NAME "uneval"
)
