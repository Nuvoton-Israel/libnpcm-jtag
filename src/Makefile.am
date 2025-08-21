bin_PROGRAMS = jtag_rw

if BUILD_LOADSVF
bin_PROGRAMS += loadsvf
loadsvf_SOURCES = loadsvf.c
if STATIC_BUILD
loadsvf_LDFLAGS = -all-static
endif
endif

jtag_rw_SOURCES = jtag_rw.c
if STATIC_BUILD
jtag_rw_LDFLAGS = -all-static
endif

LDADD = ../lib/libnpcm-jtag.la
#LDADD = ../lib/libnpcm-jtag.a
AM_CFLAGS = -I../include

