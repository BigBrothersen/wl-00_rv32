K = kernel
U = user

# Tools
CC = riscv32-unknown-elf-gcc
LD = $(CC)
OBJCOPY = riscv32-unknown-elf-objcopy

# Flags
ARCH = -march=rv32ima_zicsr -mabi=ilp32
CFLAGS = -ffreestanding -nostdlib -mcmodel=medany -O2 -Wall $(ARCH) -I$K
LDFLAGS = -T $K/kernel.ld -nostdlib -ffreestanding $(ARCH)

# Source Files
C_SRC = $K/main.c $K/uart.c $K/kprint.c $K/boot.c $K/csr.c $K/mem.c \
        $K/vm.c $K/trap.c $K/spinlock.c $K/gpr.c $K/rv.c $K/proc.c $K/syscall.c
ASM_SRC = $K/boot_asm.S $K/kerneltrap.S $K/usertrap.S $K/usertrapret.S $K/swtch.S

# Object files
C_OBJ = $(C_SRC:.c=.o)
ASM_OBJ = $(ASM_SRC:.S=.o)
OBJ = $(ASM_OBJ) $(C_OBJ)

OUTPUT = kernel.elf
INITCODE_HEADER = $K/initcode.h

all: $(INITCODE_HEADER) $(OUTPUT) 

# --- KERNEL BUILD RULES ---

$(OUTPUT): $(OBJ) $K/kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

# Compile C files
$K/%.o: $K/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile Assembly files
$K/%.o: $K/%.S
	$(CC) $(CFLAGS) -c $< -o $@

# Explicit dependency: proc.c needs initcode.h to exist before compiling
$K/proc.o: $(INITCODE_HEADER)

# --- USER PROGRAM BUILD RULES ---

# 1. Compile init.S to an object file
$U/init.o: $U/init.S
	@mkdir -p $U
	$(CC) $(CFLAGS) -c $< -o $@

# 2. Link to start at 0x1000 (User Entry Point)
# We don't use kernel.ld here; we just need the text section at 0x1000.
$U/init.elf: $U/init.o
	$(LD) -nostdlib $(ARCH) -Ttext 0x1000 -o $@ $<

# 3. Strip ELF headers to create a flat binary
$U/init.bin: $U/init.elf
	$(OBJCOPY) -S -O binary $< $@

# 4. Convert Binary to C Header (Hex Dump)
# Requires 'xxd' tool (standard on Linux/WSL)
# This creates 'unsigned char user_init_bin[]' inside initcode.h
$(INITCODE_HEADER): $U/init.bin
	xxd -i $< > $@

# --- CLEAN ---

clean:
	rm -f $K/*.o $(OUTPUT) $(INITCODE_HEADER) $U/*.o $U/*.elf $U/*.bin

.PHONY: all clean run