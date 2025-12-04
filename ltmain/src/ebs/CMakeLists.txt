

file(GLOB EBS_SOURCES_FILES lt_datacall.c 
lt_datacall.h 
lt_ebs.c 
lt_ebs.h 
ebs.h 
ebm/*.h 
ebm/*.c 
param/*.h 
param/*.c 
protocol/*.h 
protocol/*.c 
protocol/ebs_cmd/*.h 
protocol/ebs_cmd/*.c 
protocol/fm/*.h 
protocol/fm/*.c 
protocol/ip/*.h 
protocol/ip/*.c 
util/*.h 
util/*.c 
player/*.h 
player/*.c
ssm/include/*.h
ssm/include/polarssl/*.h
ssm/src/*.c)

set(EBS_SOURCES_FILES ${EBS_SOURCES_FILES} PARENT_SCOPE)
set(EBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/ebm
${CMAKE_CURRENT_SOURCE_DIR}/param
${CMAKE_CURRENT_SOURCE_DIR}/protocol
${CMAKE_CURRENT_SOURCE_DIR}/protocol/ebs_cmd
${CMAKE_CURRENT_SOURCE_DIR}/protocol/fm
${CMAKE_CURRENT_SOURCE_DIR}/protocol/ip
${CMAKE_CURRENT_SOURCE_DIR}/util
${CMAKE_CURRENT_SOURCE_DIR}/player
${CMAKE_CURRENT_SOURCE_DIR}/ssm/include
${CMAKE_CURRENT_SOURCE_DIR}/ssm/include/polarssl PARENT_SCOPE)

