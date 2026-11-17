ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

include $(PSL1GHT)/ppu_rules

TARGET      := amongus_ps3
TITLE       := Among Us PS3
APPID       := AMNG00001
CONTENTID   := UP0001-AMNG00001_00-0000000000000001
ICON0       := ICON0.PNG
PKG_DIR     := pkg
SRC_DIR     := source

CFLAGS      := -O2 -Wall -std=gnu++11 -I$(PSL1GHT)/ppu/include -I$(SRC_DIR)
LDFLAGS     := -L$(PSL1GHT)/ppu/lib -L$(PSL1GHT)/ppu/lib/psl1ght
LIBS        := -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm -lnet -lstdc++

OBJS        := $(SRC_DIR)/main.o $(SRC_DIR)/game.o $(SRC_DIR)/render.o

all: $(TARGET).self

$(TARGET).elf: $(OBJS)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TARGET).self: $(TARGET).elf
	make_self $< $@

pkg: $(TARGET).self
	@mkdir -p $(PKG_DIR)/USRDIR
	@cp $(TARGET).self $(PKG_DIR)/USRDIR/EBOOT.BIN
	@if [ -f $(ICON0) ]; then cp $(ICON0) $(PKG_DIR)/ICON0.PNG; fi
	@cat << 'EOF' > sfo.xml
<?xml version="1.0" encoding="UTF-8"?>
<sfo>
  <value name="APP_VER" type="string" max_len="8">01.00</value>
  <value name="ATTRIBUTE" type="integer">32</value>
  <value name="BOOTABLE" type="integer">1</value>
  <value name="CATEGORY" type="string" max_len="4">HG</value>
  <value name="LICENSE" type="string" max_len="512">Homebrew PS3</value>
  <value name="PARENTAL_LEVEL" type="integer">1</value>
  <value name="PS3_SYSTEM_VER" type="string" max_len="8">03.4000</value>
  <value name="RESOLUTION" type="integer">63</value>
  <value name="SOUND_FORMAT" type="integer">279</value>
  <value name="TITLE" type="string" max_len="128">Among Us PS3</value>
  <value name="TITLE_ID" type="string" max_len="16">AMNG00001</value>
  <value name="VERSION" type="string" max_len="8">01.00</value>
</sfo>
EOF
	@$(SFO) sfo.xml $(PKG_DIR)/PARAM.SFO
	@$(PKG) --contentid $(CONTENTID) $(PKG_DIR)/ $(TARGET).pkg

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_DIR)/*.o *.elf *.self *.pkg sfo.xml
	rm -rf $(PKG_DIR)

.PHONY: all pkg clean
