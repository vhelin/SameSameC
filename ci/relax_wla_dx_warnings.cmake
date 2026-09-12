# WLA DX is a dependency here. Older submodule snapshots fail under Clang
# when -pedantic-errors promotes missing prototypes to errors.
if(NOT MSVC)
  add_compile_options(-Wno-error -Wno-pedantic -Wno-strict-prototypes -Wno-deprecated-non-prototype)
endif()
