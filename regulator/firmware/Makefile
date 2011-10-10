MCU = atmega88p
F_CPU = 8000000
TARGET = regulator
FORMAT = ihex
SRC = regulator.c
DEBUG = dwarf-2
CDEFS = -DF_CPU=$(F_CPU)UL
CINCS =
OPT = s

AVRDUDE_PROGRAMMER = stk200
AVRDUDE_PORT = /dev/parport1
AVRDUDE_WRITE_FLASH = -U flash:w:$(TARGET).hex
AVRDUDE_RATE = -B 57600


OBJ = $(SRC:.c=.o) $(ASRC:.S=.o)
LST = $(SRC:.c=.lst) $(ASRC:.S=.lst)

CC = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
NM = avr-nm
AVRDUDE = avrdude
REMOVE = rm -f

CFLAGS = -g$(DEBUG)
CFLAGS += $(CDEFS) $(CINCS)
CFLAGS += -O$(OPT)
CFLAGS += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
CFLAGS += -Wall -Wstrict-prototypes
CFLAGS += -Wa,-adhlns=$(<:.c=.lst)
CFLAGS += -std=gnu99

ALL_CFLAGS = -mmcu=$(MCU) -I. $(CFLAGS) $(GENDEPFLAGS)

LDFLAGS = -Wl,-Map=$(TARGET).map,--cref

AVRDUDE_FLAGS = -p $(MCU) -P $(AVRDUDE_PORT) -c $(AVRDUDE_PROGRAMMER)
AVRDUDE_FLAGS += $(AVRDUDE_NO_VERIFY)
AVRDUDE_FLAGS += $(AVRDUDE_VERBOSE)
AVRDUDE_FLAGS += $(AVRDUDE_ERASE_COUNTER)
AVRDUDE_FLAGS += $(AVRDUDE_RATE)

all: build

build: elf hex eep lss sym

elf: $(TARGET).elf
hex: $(TARGET).hex
eep: $(TARGET).eep
lss: $(TARGET).lss
sym: $(TARGET).sym

program: $(TARGET).hex $(TARGET).eep
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_WRITE_FLASH)

%.hex: %.elf
	$(OBJCOPY) -O $(FORMAT) -R .eeprom $< $@

%.eep: %.elf
	-$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O $(FORMAT) $< $@

%.lss: %.elf
	$(OBJDUMP) -h -S $< > $@

%.sym: %.elf
	$(NM) -n $< > $@

%.elf: $(OBJ)
	$(CC) $(ALL_CFLAGS) $^ --output $@ $(LDFLAGS)

%.o : %.c
	$(CC) -c $(ALL_CFLAGS) $< -o $@

clean:
	$(REMOVE) $(TARGET).hex
	$(REMOVE) $(TARGET).eep
	$(REMOVE) $(TARGET).cof
	$(REMOVE) $(TARGET).elf
	$(REMOVE) $(TARGET).map
	$(REMOVE) $(TARGET).sym
	$(REMOVE) $(TARGET).lss
	$(REMOVE) $(OBJ)
	$(REMOVE) $(LST)
	$(REMOVE) $(SRC:.c=.s)
	$(REMOVE) $(SRC:.c=.d)
	$(REMOVE) .dep/*

-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)


