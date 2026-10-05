# Plants vs. Zombies (J2ME) -> PSP / PC port.
#
#   make JAR=/path/to/Plants_vs._Zombies_4.6.0_LockedRes_320x480_Touch_En.jar        PC build
#   make -f Makefile.psp JAR=...                                                       PSP EBOOT
#
# The game itself is not part of this repository: the original .jar is translated to C++
# (build/gen) and its data files are embedded at build time.

JAR ?= game.jar
BUILD ?= build/pc
GEN := build/gen
LIBCLS := build/libcls

PYTHON ?= python3
JAVAC ?= javac
CXX ?= g++

JAVA_SRC := $(shell find runtime/java -name '*.java')
RUNTIME_SRC := runtime/jvm.cpp runtime/natives_lang.cpp runtime/natives_lcdui.cpp runtime/natives_media.cpp \
               runtime/port.cpp runtime/main.cpp runtime/midi.cpp game/pvz_port.cpp
PLATFORM_SRC := platform/platform_sdl.cpp

CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=gnu++17 -fwrapv -fno-strict-aliasing -Wno-invalid-offsetof -Iruntime -I$(GEN) \
            $(shell sdl2-config --cflags)
LDLIBS += $(shell sdl2-config --libs) -lm -lpthread

all: $(BUILD)/pvz

# 1. mini CLDC/MIDP class library
$(LIBCLS)/.stamp: $(JAVA_SRC)
	rm -rf $(LIBCLS) && mkdir -p $(LIBCLS)
	cd runtime/java && $(JAVAC) -nowarn -source 8 -target 8 -bootclasspath . -sourcepath . \
	    -d ../../$(LIBCLS) $(patsubst runtime/java/%,%,$(JAVA_SRC))
	touch $@

# 2. bytecode -> C++
$(GEN)/.stamp: $(LIBCLS)/.stamp $(JAR) tools/jvm2cpp.py tools/classfile.py game/overrides.txt
	mkdir -p $(GEN)
	$(PYTHON) tools/jvm2cpp.py --out $(GEN) --main Game --override game/overrides.txt $(LIBCLS) $(JAR)
	touch $@

# 3. game data
build/resources.bin: $(JAR) tools/pack_resources.py
	mkdir -p build
	$(PYTHON) tools/pack_resources.py $(JAR) $@

-include $(GEN)/sources.mk

GEN_OBJ = $(patsubst %.cpp,$(BUILD)/gen/%.o,$(GEN_SOURCES))
OBJ = $(GEN_OBJ) $(patsubst %.cpp,$(BUILD)/%.o,$(RUNTIME_SRC) $(PLATFORM_SRC)) $(BUILD)/runtime/resources.o

$(BUILD)/pvz: $(GEN)/.stamp
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) link

link: $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(BUILD)/pvz $(OBJ) $(LDLIBS)

$(BUILD)/gen/%.o: $(GEN)/%.cpp $(GEN)/.stamp runtime/jvm.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/runtime/resources.o: runtime/resources.cpp build/resources.bin
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DRESOURCE_BLOB=$(abspath build/resources.bin) -c $< -o $@

$(BUILD)/%.o: %.cpp $(GEN)/.stamp $(wildcard runtime/*.h)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build

.PHONY: all link clean
