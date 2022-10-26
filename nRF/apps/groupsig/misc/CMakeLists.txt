add_library (misc
  misc.c 
  misc.h
  profile.c 
  profile.h)

target_include_directories (misc
  PUBLIC
  ${OPENSSL_INCLUDE_DIR})

target_link_libraries (misc PUBLIC base64 m)
