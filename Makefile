# Plants vs. Zombies PSP - Makefile (pspdev / PSPSDK)
TARGET = pvz
OBJS = src/main.o src/gfx.o src/audio.o src/input.o src/game.o src/reanim.o src/board.o

INCDIR = src
CFLAGS = -O2 -G0 -Wall -ffast-math
CFLAGS += $(EXTRA)
ifdef AUTOTEST
CFLAGS += -DAUTOTEST=$(AUTOTEST)
endif
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgu -lpspmp3 -lpspaudio -lpsputility -lpsppower -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Plantas contra Zombis
ifneq ($(wildcard ICON0.PNG),)
PSP_EBOOT_ICON = ICON0.PNG
endif

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
