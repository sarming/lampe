TARGET=blink
# MCU=attiny85
MCU=attiny25
SOURCES=blink.c

PROGRAMMER=dragon_isp
# PORT=-P/dev/ttyS0
# BAUD=-B115200

OBJECTS=$(SOURCES:.c=.o)
CFLAGS=-c -Os -Wall
LDFLAGS=

all: hex eeprom

hex: $(TARGET).hex

eeprom: $(TARGET)_eeprom.hex

$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex -j .data -j .text $(TARGET).elf $(TARGET).hex

$(TARGET)_eeprom.hex: $(TARGET).elf
	avr-objcopy -O ihex -j .eeprom --change-section-lma .eeprom=1 $(TARGET).elf $(TARGET)_eeprom.hex

$(TARGET).elf: $(OBJECTS)
	avr-gcc $(LDFLAGS) -mmcu=$(MCU) $(OBJECTS) -o $(TARGET).elf

.c.o:
	avr-gcc $(CFLAGS) -mmcu=$(MCU) $< -o $@

.c.s:
	avr-gcc $(CFLAGS) -S -mmcu=$(MCU) $< -o $@

size:
	avr-size --mcu=$(MCU) -C $(TARGET).elf

program:
	avrdude -p$(MCU) $(PORT) $(BAUD) -c$(PROGRAMMER) -Uflash:w:$(TARGET).hex:a

simul:
	simulavr -d $(MCU) --file $(TARGET).elf

clean_tmp:
	rm -rf *.s
	rm -rf *.o
	rm -rf *.elf

clean:
	rm -rf *.o
	rm -rf *.elf
	rm -rf *.hex
