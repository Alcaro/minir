PROGRAM = minir
ARGUI = 1
ARWUTF = 1

SOURCES_RARCH = RetroArch/gfx/drivers/xshm_gfx.c
CFLAGS_RARCH = -IRetroArch/libretro-common/include
CONF_CFLAGS = -IRetroArch/libretro-common/include
DOMAINS += RARCH
include arlib/Makefile
