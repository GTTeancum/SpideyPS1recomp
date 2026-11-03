XBE_TITLE = OpenSpidey
OUTPUT_DIR = build/release
NXDK_DIR ?= /c/nxdk

# Generated game code: make it with RecompOne's --c output (see README) and the
# default hooks with tools/gen_hook_defaults.py before building.
GAMES = spiderman

SRCS = $(wildcard $(CURDIR)/src/*.c) $(wildcard $(CURDIR)/runtime/*.c) \
       $(foreach g,$(GAMES),$(wildcard $(CURDIR)/generated/$(g)/*.c)) \
       $(CURDIR)/build/hooks_default.c

NXDK_CFLAGS += -I$(CURDIR)/runtime $(foreach g,$(GAMES),-I$(CURDIR)/generated/$(g)) \
               -O2 -Wno-unused-label -Wno-unused-variable

# The link map locates g_status_text for the XEMU monitor (tools/run_xemu.py).
NXDK_LDFLAGS += -map:build/OpenSpidey.map

include $(NXDK_DIR)/Makefile
