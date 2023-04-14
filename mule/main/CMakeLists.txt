idf_component_register(SRCS "main.c" "misc.c" "peer.c" "util.c"
                    INCLUDE_DIRS "")

#target_link_libraries(${COMPONENT_LIB} mbedtls_test)