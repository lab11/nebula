# libsys
add_library (sys
  mem.c
  sysenv.c
  mem.h)
target_include_directories (sys
  PUBLIC
  ${OPENSSL_INCLUDE_DIR})
