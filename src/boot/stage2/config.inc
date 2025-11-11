; Конфигурация Stage2

; Адреса памяти
STAGE2_LOAD_ADDR equ 0x8000
KERNEL_LOAD_ADDR equ 0x100000
STACK_ADDR equ 0x90000

; Параметры диска
KERNEL_START_SECTOR equ 11
KERNEL_SECTOR_COUNT equ 40

; Размеры
PAGE_SIZE equ 4096
SECTOR_SIZE equ 512