#pragma once

#define EMB_ARCH_CUSTOM 0
#define EMB_ARCH_UNIX 1
#define EMB_ARCH_WIN32 2
#define EMB_ARCH_ESP32 3
#define EMB_ARCH_ESP8266 4
#define EMB_ARCH_FREERTOS_TCP 5
#define EMB_ARCH_FREERTOS_LWIP 6
#define EMB_ARCH_AZURERTOS 7
#define EMB_ARCH_RTX_LWIP 8
#define EMB_ARCH_ZEPHYR 9
#define EMB_ARCH_NEWLIB 10
#define EMB_ARCH_RTX 11
#define EMB_ARCH_TIRTOS 12

#if !defined(EMB_ARCH)
#if defined(__unix__) || defined(__APPLE__)
#define EMB_ARCH EMB_ARCH_UNIX
#elif defined(_WIN32)
#define EMB_ARCH EMB_ARCH_WIN32
#elif defined(ICACHE_FLASH) || defined(ICACHE_RAM_ATTR)
#define EMB_ARCH EMB_ARCH_ESP8266
#elif defined(ESP_PLATFORM)
#define EMB_ARCH EMB_ARCH_ESP32
#elif defined(FREERTOS_IP_H)
#define EMB_ARCH EMB_ARCH_FREERTOS_TCP
#elif defined(AZURE_RTOS_THREADX)
#define EMB_ARCH EMB_ARCH_AZURERTOS
#elif defined(__ZEPHYR__)
#define EMB_ARCH EMB_ARCH_ZEPHYR
#endif

#if !defined(EMB_ARCH)
#error "EMB_ARCH is not specified and we couldn't guess it. Set -D EMB_ARCH=..."
#endif
#endif  // !defined(EMB_ARCH)

#if EMB_ARCH == EMB_ARCH_CUSTOM
#include <mongoose_custom.h>
#endif

#include "arch_esp32.h"
#include "arch_esp8266.h"
#include "arch_freertos_lwip.h"
#include "arch_freertos_tcp.h"
#include "arch_newlib.h"
#include "arch_rtx.h"
#include "arch_rtx_lwip.h"
#include "arch_unix.h"
#include "arch_win32.h"
#include "arch_zephyr.h"
