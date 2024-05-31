##########################################################
# PROPRIETARY RIGHTS NOTICE:
#
# All rights reserved. This material contains the valuable properties and trade
# secrets of
#
# Kitu Systems, Inc.
#
# Copyright © 2019-2025
# Kitu Systems, Inc.
##########################################################


cmake_minimum_required (VERSION 2.8.12)
project (Proj_KituSparkClient)

# Where is the target environment
# Release and Debug build
#SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wsign-compare -Wconversion")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wsign-compare -Wconversion")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

# Targets
add_compile_options(
    "$<$<CONFIG:DEBUG>:-g>"
    "$<$<CONFIG:RELEASE>:-Os>"
)

#SET (CMAKE_C_COMPILER   /opt/rpi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-gcc)
#SET (CMAKE_CXX_COMPILER /opt/rpi/tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-g++)
SET (CMAKE_C_COMPILER   /home/viswanath/Documents/EPRI/spin/mevse_toolchain/arm-2009q1/bin/arm-none-linux-gnueabi-gcc)
SET (CMAKE_CXX_COMPILER /home/viswanath/Documents/EPRI/spin/mevse_toolchain/arm-2009q1/bin/arm-none-linux-gnueabi-g++)

################################################################################
# Source Code
################################################################################
# Application
set (APP_SOURCES
    ./app/source/app.c
    ./app/source/app_client.c
    ./app/source/app_dcap_client.c
    ./app/source/app_der_client.c
    ./app/source/app_eder_client.c
    ./app/source/app_edev_client.c
    ./app/source/app_edinfo_client.c
    ./app/source/app_elel_client.c
    ./app/source/app_epwrstat_client.c
    ./app/source/app_fsa_client.c
    ./app/source/app_meter_mirror_client.c
    ./app/source/app_sder_client.c
    ./app/source/app_sdev_client.c
    ./app/source/app_tables.c
    ./app/source/app_time_client.c
    ./app/source/app_utils.c
    ./app/source/app_process/app_comm.c
    ./app/source/app_process/app_logs.c
    ./app/source/usock_proto.c
    ./app/source/usock_client.c
    ./app/source/usock_utils.c
    ./app/source/utils.c
)

set (PLATFORM_SOURCES
    ./kitu_api/source/klnx_alarm.c
    ./kitu_api/source/klnx_memory.c
    ./kitu_api/source/klnx_message.c
    ./kitu_api/source/klnx_misc.c
    ./kitu_api/source/klnx_mm.c
    ./kitu_api/source/klnx_semaphore.c
    ./kitu_api/source/klnx_swd.c
    ./kitu_api/source/klnx_task.c
    ./kitu_api/source/klnx_timebase.c
    ./kitu_api/source/klnx_timer.c
    ./kitu_api/source/psal_port_linux.c
    ./kitu_api/source/psal_failfile.c
    ./kitu_api/source/psal_sighandler.c
)

set (Kitu_Includes
    "app/include"
    "kitu_api/include"
    "kitu_api/source"
)

# Defines
add_definitions (-DSTDLIB_BUILD)
add_definitions (-DUSE_SEPARATE_APP_PROCESS)

# Eanble APP
add_definitions (-DAPP_CONFIG_DCAP_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_EDINFO_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_DER_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_EDER_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_EDEV_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_FSA_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_ELEL_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_METER_MIRROR_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_EPS_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_RESP_CLIENT_ENABLED=1)
add_definitions (-DAPP_CONFIG_TIME_CLIENT_ENABLED=1)
add_definitions (-DOVERRIDE_APP_AUTO_START=1)
add_definitions (-DOVERRIDE_APP_PRELOAD_DEMO=1)
add_definitions (-DOVERRIDE_ISAL_CONFIG_VER=ISAL_CONFIG_V4)
add_definitions (-DOVERRIDE_SL_OVERRIDE_SERVER_ENABLE=0)
add_definitions (-DOVERRIDE_SL_OVERRIDE_CLIENT_ENABLE=1)
add_definitions (-DAPP_CONFIG_SDER_CLIENT_ENABLED=1)

# App Process
    add_definitions (-DUSE_SEPARATE_APP_PROCESS)

    set (KituApp_Sources ${APP_SOURCES} ${PLATFORM_SOURCES})
    set (APP_PROCESS_NAME "kituclient_process")
    add_executable(${APP_PROCESS_NAME} ${KituApp_Sources})

    target_include_directories(${APP_PROCESS_NAME} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
    # Kitu Includes
        ${Kitu_Includes}
        "./app/source/app_process"
    )
    target_link_libraries(${APP_PROCESS_NAME} m rt pthread)


ADD_CUSTOM_TARGET(debug
  COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Debug ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
  COMMENT "Creating the executable in the debug mode.")

ADD_CUSTOM_TARGET(release
  COMMAND ${CMAKE_COMMAND} -DCMAKE_BUILD_TYPE=Release ${CMAKE_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target all
  COMMENT "Creating the executable in the release mode.")

if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
    message("debug mode")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/debug)
endif (CMAKE_BUILD_TYPE EQUAL "DEBUG")
