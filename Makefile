ifeq ($(strip $(PSL1GHT)),)
$(error "Defina a variavel PSL1GHT")
endif
include $(PSL1GHT)/ppu_rules

.DEFAULT_GOAL := all

TARGET    := amongus_ps3
TITLE     := Among Us PS3
APPID     := AMNG00001
CONTENTID := UP0001-$(APPID)_00-0000000000000000
SFOXML    := sfo.xml
PORTLIBS  := $(PS3DEV)/portlibs/ppu
SRC_DIR   := source

MACHDEP   := -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections
CXXFLAGS  := -O2 -Wall -std=gnu++11 -mcpu=cell $(MACHDEP) -I$(PSL1GHT)/ppu/include -I$(PORTLIBS)/include -I$(SRC_DIR)
LDFLAGS   := $(MACHDEP) -L$(PSL1GHT)/ppu/lib -L$(PORTLIBS)/lib
LIBS      := -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm -lstdc++

OBJS      := $(SRC_DIR)/main.o $(SRC_DIR)/game.o $(SRC_DIR)/render.o

all: $(TARGET).pkg

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(SFOXML):
	@if [ -f $(PSL1GHT)/bin/sfo.xml ]; then cp $(PSL1GHT)/bin/sfo.xml $(SFOXML); \
	elif [ -f $(PSL1GHT)/sfo.xml ]; then cp $(PSL1GHT)/sfo.xml $(SFOXML); \
	else printf '<?xml version="1.0" encoding="UTF-8"?>\n<sfo>\n  <value name="APP_VER" type="string" max_len="8">01.00</value>\n  <value name="ATTRIBUTE" type="integer">32</value>\n  <value name="BOOTABLE" type="integer">1</value>\n  <value name="CATEGORY" type="string" max_len="4">HG</value>\n  <value name="LICENSE" type="string" max_len="512">Homebrew PS3</value>\n  <value name="PARENTAL_LEVEL" type="integer">1</value>\n  <value name="PS3_SYSTEM_VER" type="string" max_len="8">03.4000</value>\n  <value name="RESOLUTION" type="integer">63</value>\n  <value name="SOUND_FORMAT" type="integer">279</value>\n  <value name="TITLE" type="string" max_len="128">Homebrew</value>\n  <value name="TITLE_ID" type="string" max_len="16">HOME00001</value>\n  <value name="VERSION" type="string" max_len="8">01.00</value>\n</sfo>\n' > $(SFOXML); fi

$(TARGET).pkg: $(TARGET).elf ICON0.PNG $(SFOXML)
	rm -rf pkg
	mkdir -p pkg/USRDIR
	cp ICON0.PNG pkg/ICON0.PNG
	$(PS3DEV)/bin/sprxlinker $(TARGET).elf
	$(PS3DEV)/bin/make_self_npdrm $(TARGET).elf pkg/USRDIR/EBOOT.BIN $(CONTENTID)
	$(PS3DEV)/bin/sfo.py --title "$(TITLE)" --appid "$(APPID)" -f $(SFOXML) pkg/PARAM.SFO
	$(PS3DEV)/bin/pkg.py --contentid $(CONTENTID) pkg/ $@

clean:
	rm -rf pkg $(SRC_DIR)/*.o *.elf *.self *.pkg
