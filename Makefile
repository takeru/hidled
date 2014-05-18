## General Flags
PROJECT = hidled
MCU = atmega88p
F_CPU = 20000000UL
#F_CPU = 12000000UL
TARGET = $(PROJECT).elf
CC = avr-gcc

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -DF_CPU=$(F_CPU) -Os -fsigned-char
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS += 

## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

USBDRV=./v-usb/usbdrv

## Include Directories
INCLUDES = -I"." -I"$(USBDRV)" 

## Objects that must be built in order to link
OBJECTS = main.o usbdrvasm.o oddebug.o usbdrv.o 

## Objects explicitly added by the user
LINKONLYOBJECTS = 

CONFIG = usbconfig.h Makefile

## Build
all: $(TARGET) $(PROJECT).hex $(PROJECT).eep $(PROJECT).lss## Compile
usbdrvasm.o: $(USBDRV)/usbdrvasm.S $(CONFIG)
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

main.o: main.c $(CONFIG)
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

oddebug.o: $(USBDRV)/oddebug.c $(CONFIG)
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

usbdrv.o: $(USBDRV)/usbdrv.c $(CONFIG)
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) $(PROJECT).elf dep/* $(PROJECT).hex $(PROJECT).eep $(PROJECT).lss

## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)
