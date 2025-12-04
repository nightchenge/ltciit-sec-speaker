# Copyright (C) 2020 QUECTEL Technologies Limited and/or its affiliates("QUECTEL").
# All rights reserved.
#

add_subdirectory(src/ebs)

set(target ltmain)

add_library(${target} STATIC)
set_target_properties(${target} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${out_app_lib_dir})
target_compile_definitions(${target} PRIVATE OSI_LOG_TAG=LOG_TAG_QUEC)
target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc ${EBS_INCLUDE_DIRS})

add_library(ql_tts_api STATIC IMPORTED)
set_target_properties(ql_tts_api PROPERTIES IMPORTED_LOCATION ${SOURCE_TOP_DIR}/components/newlib/armca5/libql_api_tts.a)

target_sources(${target} PRIVATE
	ltmain.c
	ltplay.c
	src/ltkey/ltkey.c
	src/ltkey/ltkeydrv.c
	src/ltkey/ltqueue.c	
	src/ltuart/ec800g_ota_master.c
	src/ltuart/ltuart2frx8016.c
	src/ltmp3/ltmp3.c
	src/ltmp3/ltaudio.c
	src/ltsdmmc/ltsdmmc.c
	src/ltsms/ltsms.c
	src/ltsms/ltsms_t.c
	src/ltsms/ltclk_t.c
	src/ltkey/ltadc.c
	src/ltvoicecall/ltvoicecall.c
	src/ltfm/fm8035.c
	src/ltfm/ltfm.c
	src/ltled/led.c
	src/ltsystem/ltsystem.c
	src/ltmqtt/ltmqtt.c
	src/ltconfig/cJSON.c
	src/ltconfig/ltconf.c
	src/ltrecord/ltrecord.c	
	src/ltpower/ltpower.c	
	src/ltwebaudio/ltweb_audio.c
	src/ltmp3/ltaudiourl.c
	src/ltfm/ltfmurl.c
	src/lthttp/lt_fota_http.c
	src/lthttp/lt_http_post.c
	src/lthttp/lt_http_put.c
	src/lthttp/lt_https_get.c
	src/lthttp/lt_http_demo.c
	src/lttts/lt_tts_demo.c
	src/lttts/lt_tts_demo2.c
	src/lsm6ds3tr/lsm6ds3tr.c
	src/lsm6ds3tr/lsm6ds3tr_c_reg.c
	src/ltkey/volume_knob_example.c
	src/dynamic_config/dynamic_config.c
	
)
target_sources(${target} PRIVATE ${EBS_SOURCES_FILES})

relative_glob(srcs include/*.h src/*.c inc/*.h src/ltkey/*.c ${EBS_SOURCES_FILES})


# target_link_libraries(${target} PRIVATE ebs)

target_link_libraries(${target} PRIVATE   ql_tts_api ql_app_peripheral)
beautify_c_code(${target} ${srcs})

