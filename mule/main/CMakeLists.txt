idf_component_register(SRCS "main.c" "misc.c" "peer.c" "util.c" "../../ext/base64/base64.c"
                    INCLUDE_DIRS "../../ext/base64/"
                    EMBED_TXTFILES app_and_server_cert.pem)

#target_link_libraries(${COMPONENT_LIB} mbedtls_test)
