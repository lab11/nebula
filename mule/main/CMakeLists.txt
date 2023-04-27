idf_component_register(SRCS "main.c" "misc.c" "peer.c" "util.c" "../../ext/base64/base64.c"
                    INCLUDE_DIRS "../../ext/base64/")

#target_link_libraries(${COMPONENT_LIB} mbedtls_test)