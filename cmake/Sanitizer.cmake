# -------------------------------------------------------------
# target_add_sanitizer_flags : add address/memory sanitizer flags to target
# -------------------------------------------------------------
function(target_add_sanitizer_flags target)
    target_compile_options(
      ${target}
      PRIVATE -fsanitize=address
              -fsanitize=undefined
              -fno-sanitize-recover=all
              -fsanitize=float-divide-by-zero
              -fsanitize=float-cast-overflow
              -fno-sanitize=null
              -fno-sanitize=alignment
    )
    target_link_options(
      ${target}
      PRIVATE
      -fsanitize=address
      -fsanitize=undefined
      -fno-sanitize-recover=all
      -fsanitize=float-divide-by-zero
      -fsanitize=float-cast-overflow
      -fno-sanitize=null
      -fno-sanitize=alignment
    )
endfunction()
