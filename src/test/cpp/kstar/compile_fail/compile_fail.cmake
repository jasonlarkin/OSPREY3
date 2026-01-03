cmake_minimum_required(VERSION 3.20)

# NOTE: This file is executed via `cmake -P`, where `try_compile()` is not available.
# Implement compile-fail tests by invoking the C++ compiler directly and asserting failure.

if (NOT DEFINED REPO_ROOT)
  message(FATAL_ERROR "REPO_ROOT not set")
endif()
if (NOT DEFINED BUILD_DIR)
  message(FATAL_ERROR "BUILD_DIR not set")
endif()
if (NOT DEFINED CXX_COMPILER)
  message(FATAL_ERROR "CXX_COMPILER not set")
endif()

set(_src_dir "${REPO_ROOT}/src/test/cpp/kstar/compile_fail")
set(_inc_main "${REPO_ROOT}/src/main/cpp/kstar")

file(MAKE_DIRECTORY "${BUILD_DIR}/compile-fail")

function(expect_compile_fail name src extra_flags)
  set(_out_dir "${BUILD_DIR}/compile-fail/${name}")
  file(MAKE_DIRECTORY "${_out_dir}")

  set(_obj "${_out_dir}/${name}.o")

  # Compile exactly one translation unit. We do NOT link.
  execute_process(
    COMMAND "${CXX_COMPILER}"
            -std=c++20
            -Wall -Wextra
            -I "${_inc_main}"
            ${extra_flags}
            -c "${src}"
            -o "${_obj}"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )

  if (_rc EQUAL 0)
    message(FATAL_ERROR
      "Expected compile FAIL but it SUCCEEDED for ${name}\n"
      "Compiler: ${CXX_COMPILER}\n"
      "Source: ${src}\n"
      "stdout:\n${_stdout}\n"
      "stderr:\n${_stderr}\n"
    )
  else()
    message(STATUS "Compile-fail OK: ${name}")
    # Delete any produced object file to avoid polluting the build tree.
    file(REMOVE "${_obj}")
  endif()
endfunction()

expect_compile_fail(
  "fail_partition_function_int"
  "${_src_dir}/fail_partition_function_int.cpp"
  ""
)

# Force [[nodiscard]] to be treated as an error.
expect_compile_fail(
  "fail_partition_function_nodiscard"
  "${_src_dir}/fail_partition_function_nodiscard.cpp"
  "-Werror;-Werror=unused-result"
)

