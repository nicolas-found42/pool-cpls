# Sanitizer presets. Selected only when the caller sets POOL_SANITIZE.
#
#   POOL_SANITIZE=address   -> ASan
#   POOL_SANITIZE=undefined -> UBSan (non-recovering, prints a stack)
#   POOL_SANITIZE=thread    -> TSan (mutually exclusive with ASan)
#   POOL_SANITIZE=address,undefined -> combined, the usual CI choice
#
# Apple clang supports all three; ASan and TSan cannot be combined.

function(pool_apply_sanitizers target)
  if(NOT POOL_SANITIZE)
    return()
  endif()

  if(POOL_SANITIZE MATCHES "thread" AND POOL_SANITIZE MATCHES "address")
    message(FATAL_ERROR "POOL_SANITIZE: thread and address sanitizers cannot be combined")
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(WARNING "Sanitizers requested for ${CMAKE_CXX_COMPILER_ID}; flags may be unsupported")
  endif()

  target_compile_options(${target} INTERFACE
    -fsanitize=${POOL_SANITIZE}
    -fno-omit-frame-pointer
    -fno-sanitize-recover=all)

  target_link_options(${target} INTERFACE
    -fsanitize=${POOL_SANITIZE}
    -fno-omit-frame-pointer)
endfunction()
