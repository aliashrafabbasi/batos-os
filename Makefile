CC = gcc
LD = ld
NASM = nasm

CFLAGS = -Wall -Wextra -O2 \
         -ffreestanding \
         -fno-stack-protector \
         -fno-pie \
         -fno-pic \
         -mno-sse \
         -mno-sse2 \
         -mno-mmx \
         -mno-80387 \
         -mno-red-zone \
         -mcmodel=kernel \
         -I.

LDFLAGS = -T kernel/linker.ld -nostdlib -static

BUILD_DIR = build
ISO_DIR = iso_root

C_SOURCES = \
    kernel/kernel.c \
    kernel/arch/x86_64/gdt.c \
    kernel/arch/x86_64/idt.c \
	kernel/arch/x86_64/tss.c \


ASM_SOURCES = \
    kernel/arch/x86_64/interrupts.asm

C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(BUILD_DIR)/interrupts.o

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

.PHONY: all clean iso

all: $(BUILD_DIR)/batos.iso

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/interrupts.o: kernel/arch/x86_64/interrupts.asm
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/batos.elf: $(OBJECTS)
	mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/batos.iso: $(BUILD_DIR)/batos.elf
	mkdir -p $(ISO_DIR)/boot
	mkdir -p $(ISO_DIR)/EFI/BOOT

	cp $(BUILD_DIR)/batos.elf $(ISO_DIR)/boot/batos.elf
	cp limine.conf $(ISO_DIR)/

	cp limine/limine-bios-cd.bin $(ISO_DIR)/boot/
	cp limine/limine-bios.sys $(ISO_DIR)/boot/
	cp limine/limine-uefi-cd.bin $(ISO_DIR)/boot/
	cp limine/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/

	xorriso -as mkisofs \
		-b boot/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		-o $@ \
		$(ISO_DIR)

	./limine/limine bios-install $@

clean:
	rm -f $(C_OBJECTS) $(ASM_OBJECTS)
	rm -f $(BUILD_DIR)/batos.elf
	rm -f $(BUILD_DIR)/batos.iso
	rm -f $(ISO_DIR)/boot/batos.elf
	rm -f $(ISO_DIR)/boot/kernel.elf