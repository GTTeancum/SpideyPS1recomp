XBE_TITLE = OpenSpidey
NXDK_DIR ?= /c/nxdk

# One game per XBE: make GAME=spiderman (the default) or GAME=spiderman2. Each game's
# code is about 10 MB, more than the console has spare for a second one, and the two
# games define hooks of the same names. The runtime and the shared sources are common;
# src/*_sm1.c belong to Spider-Man 1 and src/*_sm2.c to Spider-Man 2.
GAME ?= spiderman
ifeq ($(GAME),spiderman)
GAME_TAG = sm1
OUTPUT_DIR = build/release
else ifeq ($(GAME),spiderman2)
GAME_TAG = sm2
OUTPUT_DIR = build/sm2
else
$(error GAME must be spiderman or spiderman2)
endif
OTHER_TAGS = $(filter-out $(GAME_TAG),sm1 sm2)
# SDL2's game controller API for the Xbox pad (src/input.c).
NXDK_SDL = y

# Generated game code: make it with RecompOne's --c output (see README) and the
# default hooks with tools/gen_hook_defaults.py before building.
GAMES = $(GAME)

# NV2A shaders (nxdk's %.inl rules: Cg -> vp20/fp20 -> push-buffer words).
SHADER_OBJS = $(CURDIR)/runtime/ps1vs.inl $(CURDIR)/runtime/ps1ps.inl \
              $(CURDIR)/runtime/crackvs.inl $(CURDIR)/runtime/crackps.inl

SRCS = $(filter-out $(foreach t,$(OTHER_TAGS),$(CURDIR)/src/%_$(t).c),$(wildcard $(CURDIR)/src/*.c)) $(wildcard $(CURDIR)/runtime/*.c) $(wildcard $(CURDIR)/runtime/*.cpp) \
       $(wildcard $(CURDIR)/runtime/*.s) \
       $(foreach g,$(GAMES),$(wildcard $(CURDIR)/generated/$(g)/*.c)) \
       $(CURDIR)/build/hooks_default_$(GAME).c

NXDK_CFLAGS += -I$(CURDIR)/runtime $(foreach g,$(GAMES),-I$(CURDIR)/generated/$(g)) \
               -O3 -Wno-unused-label -Wno-unused-variable

# nxdk compiles freestanding (-fno-builtin): memcpy and memset are library calls (its
# own move a byte at a time; runtime/memfast.s replaces them, and recomp.h sends small
# copies to the compiler's builtins). Without errno from maths, __builtin_sqrtf is the
# SSE square root instead of a call to the library's x87 one (same result: a double
# square root rounded to single is the correctly rounded single one).
NXDK_CFLAGS += -fno-math-errno
NXDK_CXXFLAGS += -fno-math-errno
# The C++ (the PC's retarget core) needs the optimisation level too: without it the
# core ran unoptimised, every value kept in memory.
NXDK_CXXFLAGS += -O3

# Link-time optimisation: lets the GTE and memory helpers inline into the generated
# game code across files.
LTO = y

# make RT_NO_VTAGS=1: the generated code's vertex-tag work compiled out (timing only).
ifeq ($(RT_NO_VTAGS),1)
NXDK_CFLAGS += -DRT_NO_VTAGS
endif

# make RT_PROFILE=1: time breakdown in the status text (tools/xbdm.py status).
ifeq ($(RT_PROFILE),1)
NXDK_CFLAGS += -DRT_PROFILE
endif

# The link map locates g_status_text for the XEMU monitor (tools/run_xemu.py).
NXDK_LDFLAGS += -map:$(OUTPUT_DIR)/OpenSpidey.map -opt:lldlto=3

include $(NXDK_DIR)/Makefile

# nxdk links every game to the same main.exe, from every prerequisite listed: when the
# game differs from the one last linked, build/linked-game changes, src/main.obj is
# compiled again and so main.exe is linked again.
$(shell mkdir -p build $(OUTPUT_DIR); [ "$$(cat build/linked-game 2>/dev/null)" = "$(GAME)" ] || echo "$(GAME)" > build/linked-game)
$(CURDIR)/src/main.obj: build/linked-game

# Header dependencies. The compiler is a Windows program: the dependency files it
# writes beside each object (X.c.d, through nxdk's -MD) name the object and every header
# as Windows paths (Z:/..., Z:\...), which make -- with its /z/... names -- never
# matches, so a header change rebuilt nothing. Each .d is rewritten as X.c.mk with make's
# own paths (drive letter to /z/, backslashes to slashes; line continuations and
# escaped spaces, as in C:\Program\ Files, kept) when it is newer than its .mk, and those
# are included instead.
DEP_FILES := $(wildcard $(CURDIR)/src/*.d $(CURDIR)/runtime/*.d \
                        $(foreach g,$(GAMES),$(CURDIR)/generated/$(g)/*.d) $(CURDIR)/build/*.d)
DEP_MAKEFILES := $(DEP_FILES:.d=.mk)
$(shell for d in $(DEP_FILES); do m="$${d%.d}.mk"; \
          if [ ! -e "$$m" ] || [ "$$d" -nt "$$m" ]; then \
            sed -e 's/[\\] /@SPACE@/g' -e 's/[\\]$$/@CONTINUE@/' -e 's|[\\]|/|g' \
                -e 's|@CONTINUE@|\\|' -e 's|@SPACE@|\\ |g' \
                -e 's|\b\([A-Za-z]\):/|/\L\1/|g' "$$d" > "$$m"; \
          fi; done)
-include $(DEP_MAKEFILES)
