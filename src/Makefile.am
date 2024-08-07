bin_PROGRAMS = jtag_rw

if BUILD_LOADSVF
bin_PROGRAMS += loadsvf
loadsvf_SOURCES = loadsvf.c
endif

jtag_rw_SOURCES = jtag_rw.c

LDADD = ../lib/libnpcm-jtag.la
#LDADD = ../lib/libnpcm-jtag.a
AM_CFLAGS = -I../include

