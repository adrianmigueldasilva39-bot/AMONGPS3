# Makefile
ifeq ($(strip $(PSL1GHT)),)
$(error "Defina a variavel PSL1GHT")
endif
include $(PSL1GHT)/ppu_rules

TARGET    := tripulante
TITLE     := Tripulante Arena
APPID     := TRIP00001
CONTENTID := UP0001-$(APPID)_00-0000000000000000
SFOXML    := sfo.xml
PORTLIBS  := $(PS3DEV)/portlibs/ppu

CXX       := $(PS3DEV)/ppu/bin/ppu-g++
CC        := $(PS3DEV)/ppu/bin/ppu-gcc

MACHDEP := -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections
CXXFLAGS  := -O2 -Wall -mcpu=cell $(MACHDEP) -fno-exceptions -fno-rtti -I$(PSL1GHT)/ppu/include -I$(PORTLIBS)/include -Isource
CFLAGS    := -O2 -Wall -mcpu=cell $(MACHDEP) -I$(PSL1GHT)/ppu/include -I$(PORTLIBS)/include
LDFLAGS := $(MACHDEP) -L$(PSL1GHT)/ppu/lib -L$(PORTLIBS)/lib
LIBS    := -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm -lstdc++

OBJS := source/main.o source/game.o source/render.o

all: $(TARGET).pkg

source/main.o: source/main.cpp source/game.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

source/game.o: source/game.cpp source/game.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

source/render.o: source/render.cpp source/game.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(TARGET).pkg: $(TARGET).elf ICON0.PNG $(SFOXML)
	rm -rf pkg
	mkdir -p pkg/USRDIR
	cp ICON0.PNG pkg/ICON0.PNG
	$(PS3DEV)/bin/sprxlinker $(TARGET).elf
	$(PS3DEV)/bin/make_self_npdrm $(TARGET).elf pkg/USRDIR/EBOOT.BIN $(CONTENTID)
	$(PS3DEV)/bin/sfo.py --title "$(TITLE)" --appid "$(APPID)" -f $(SFOXML) pkg/PARAM.SFO
	$(PS3DEV)/bin/pkg.py --contentid $(CONTENTID) pkg/ $@

clean:
	rm -rf pkg source/*.o *.elf *.self *.pkg