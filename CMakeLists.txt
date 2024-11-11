project(ocpp_client C)
cmake_minimum_required(VERSION 3.5)
find_package(libwebsockets CONFIG REQUIRED)

require_lws_config(LWS_ROLE_H1 1 requirements)
require_lws_config(LWS_ROLE_WS 1 requirements)
require_lws_config(LWS_WITHOUT_CLIENT 0 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS 1 requirements)
require_lws_config(LWS_WITH_SECURE_STREAMS_STATIC_POLICY_ONLY 0 requirements)
require_lws_config(LWS_WITH_TLS 1 requirements)

require_lws_config(LWS_WITH_SYS_FAULT_INJECTION 1 has_fault_injection)
require_lws_config(LWS_WITH_SECURE_STREAMS_PROXY_API 1 has_ss_proxy)
require_lws_config(LWS_WITH_SYS_STATE 1 has_sys_state)

CHECK_C_SOURCE_COMPILES("#include <libwebsockets.h>\nint main(void) {\ni#if defined(LWS_WITH_SECURE_STREAMS_PROXY_API)\n return 0;\n #else\n fail\n #endif\n return 0;\n}\n" HAS_LWS_WITH_SECURE_STREAMS_PROXY_API)

if (requirements)
    #add_compile_options(-mfloat-abi=hard)
	set(SRCS main.c cJSON.c)

	add_executable(${PROJECT_NAME} ${SRCS})

	target_link_libraries(${PROJECT_NAME}
		websockets
		${LIBWEBSOCKETS_DEP_LIBS})
endif()

