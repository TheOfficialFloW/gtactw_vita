#ifndef __CONFIG_H__
#define __CONFIG_H__

// #define DEBUG

#define LOAD_ADDRESS 0x98000000

#define MEMORY_SCELIBC_MB 4
#define MEMORY_NEWLIB_MB 240
#define MEMORY_VITAGL_THRESHOLD_MB 16

#define DATA_PATH "ux0:data/gtactw"
#define SO_PATH DATA_PATH "/" "libCTW.so"
#define CONFIG_PATH DATA_PATH "/" "config.txt"

#define SCREEN_W 960
#define SCREEN_H 544

#endif
