# Compiler warnings shared by every first-party target.
#
# Policy: warnings are always on; -Werror is opt-in via POOL_WARNINGS_AS_ERRORS
# so a newer compiler does not brick a developer's local build. CI turns it on.

function(pool_apply_warnings target)
  set(clang_gcc_warnings
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wcast-align
    -Wcast-qual
    -Wundef
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -Wold-style-cast
    -Woverloaded-virtual
    -Wnon-virtual-dtor
    -Wunused
    -Wwrite-strings)

  target_compile_options(${target} INTERFACE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:${clang_gcc_warnings}>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/Zc:__cplusplus;/utf-8;/EHsc>
    # MSVC's C4458 (shadowing) and C4244 (conversion) come free with /W4.
    $<$<CXX_COMPILER_ID:MSVC>:/wd4996>)

  # GCC needs -Wno-... for a couple of pedantic-only misfires on C++23 headers.
  target_compile_options(${target} INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>)

  if(POOL_WARNINGS_AS_ERRORS)
    target_compile_options(${target} INTERFACE
      $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Werror>
      $<$<CXX_COMPILER_ID:MSVC>:/WX>)
  endif()
endfunction()
