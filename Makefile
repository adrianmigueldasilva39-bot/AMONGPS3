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
	@printf '<?xml version="1.0" encoding="UTF-8"?>\n<sfo>\n<title>%s</title>\n<title_id>%s</title_id>\n<version>01.00</version>\n<app_ver>01.00</app_ver>\n<parental_level>1</parental_level>\n</sfo>\n' "$(TITLE)" "$(APPID)" > sfo.xml
	@$(SFO) sfo.xml $(PKG_DIR)/PARAM.SFO
	@$(PKG) --contentid $(CONTENTID) $(PKG_DIR)/ $(TARGET).pkg

$(SRC_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_DIR)/*.o *.elf *.self *.pkg sfo.xml
	rm -rf $(PKG_DIR)

.PHONY: all pkg clean
