add_library(libtest-tls SHARED lib.cpp)
banan_link_library(libtest-tls libc)
target_link_options(libtest-tls PRIVATE -nolibc)
set_target_properties(libtest-tls PROPERTIES LINK_FLAGS "-Wl,-soname,libtest-tls.so")

add_executable(test-tls main.cpp)
banan_link_library(test-tls libc)
banan_link_library(test-tls libtest-tls)

install(TARGETS test-tls OPTIONAL)
install(TARGETS libtest-tls OPTIONAL)
