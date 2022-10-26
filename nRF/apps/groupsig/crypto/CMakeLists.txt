# libgcrypto
add_library (gcrypto spk.c prf.c)
target_include_directories (gcrypto
  PUBLIC
  ${OPENSSL_INCLUDE_DIR})
target_link_libraries (gcrypto PRIVATE ${OPENSSL_LIBRARIES})
