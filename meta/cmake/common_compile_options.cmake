set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(-Wall -Wextra -Wshadow -Wconversion)

# https://github.com/ninja-build/ninja/issues/174
add_compile_options(-fdiagnostics-color=always)

# FIXME: Is it still an issue?
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82283
add_compile_options(-Wno-missing-field-initializers)

if (CMAKE_BUILD_TYPE STREQUAL Release)
    add_compile_options(-O3)
else()
    add_compile_options(-O0 -g)
endif()
