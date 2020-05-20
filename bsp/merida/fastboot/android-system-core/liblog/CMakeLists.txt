set(liblog_SRCS
	log_event_list.c
	log_event_write.c
	logger_write.c
	config_write.c
	logger_name.c
	logger_lock.c
	log_ratelimit.cpp

	# host sources
	fake_log_device.c
	fake_writer.c
)

add_definitions(-Werror -fvisibility=hidden)

add_library(log ${liblog_SRCS} )
target_compile_definitions(log PUBLIC -DFAKE_LOG_DEVICE=1 -DLIBLOG_LOG_TAG=1006 -DSNET_EVENT_LOG_TAG=1397638484)
target_link_libraries(log rt pthread)
