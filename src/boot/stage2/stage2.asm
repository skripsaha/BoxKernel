[BITS 16]
[ORG 0x8000]

; Stage2 - Исправленная версия с устранением triple fault
; Подпись для проверки
dw 0x2907

start_stage2:
    ; Отключаем прерывания в начале
    cli
    
    ; Очистка направления флага
    cld
    
    ; Настройка сегментов более безопасно
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Настройка стека с выравниванием
    mov ss, ax
    mov sp, 0x7BF0    ; Стек ниже загрузчика, выровнен
    
    ; Теперь можно включить прерывания
    sti

    ; Сообщение о старте
    mov si, msg_stage2_start
    call print_string_16
    
    call wait_key
    
    ; Включение A20 (улучшенная версия)
    call enable_a20_enhanced
    
    ; Детекция памяти
    call detect_memory_e820

    ; Загрузка ядра
    call load_kernel_simple

    ; Переход в защищенный режим
    mov si, msg_entering_protected
    call print_string_16
    call wait_key
    
    ; Отключаем прерывания перед переходом
    cli
    
    ; Загрузка GDT
    lgdt [gdt_descriptor]
    
    ; Включение защищенного режима
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump для очистки pipeline
    jmp 0x08:protected_mode_start

[BITS 32]
protected_mode_start:
    ; Настройка сегментов в 32-bit режиме
    mov ax, 0x10        ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Настройка стека (выровнен по 16 байт)
    mov esp, 0x90000
    
    ; Очистка EFLAGS
    push dword 0
    popf
    
    ; Сообщение о входе в защищенный режим
    mov edi, 0xB8000
    mov al, 'P'
    mov ah, 0x0F
    mov [edi], ax
    mov al, 'M'
    mov [edi+2], ax
    
    ; Инициализация страничной адресации
    call setup_paging_fixed
    
    ; Переход в long mode
    call enable_long_mode_fixed
    
    ; Far jump в 64-bit режим
    jmp 0x18:long_mode_start

[BITS 64]
long_mode_start:
    ; Настройка сегментов в 64-bit режиме
    mov ax, 0x20        ; Используем правильный селектор данных
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Настройка стека для 64-bit (выровнен по 16 байт)
    mov rsp, 0x90000
    
    ; Очистка RFLAGS
    push qword 0
    popf
    
    ; Сообщение о входе в long mode
    mov rdi, 0xB8000
    mov al, 'L'
    mov ah, 0x0F
    mov [rdi+4], ax
    mov al, 'M'
    mov [rdi+6], ax
    
    ; Подготовка параметров для ядра
    mov rdi, 0x90000             ; Адрес e820 карты
    movzx rsi, word [0x8FFE]     ; Размер e820 карты
    
    ; Проверка загрузки ядра
    mov rax, [0x10000]
    test rax, rax
    jz .kernel_not_loaded
    
    ; Сохраняем данные для ядра
    mov [0x8000], dword 0x90000     ; Адрес E820 карты
    mov ax, [0x8FFE]
    mov [0x8004], ax                ; Размер E820 карты
    mov [0x8008], dword 0x10000     ; Адрес загрузки ядра
    mov [0x800C], dword 0x29000     ; Конец ядра (200 секторов * 512 = 100KB)
    
    ; Переход в ядро
    jmp 0x10000
    
.kernel_not_loaded:
    ; Сообщение об ошибке
    mov rdi, 0xB8000
    mov al, 'N'
    mov ah, 0x04        ; Красный цвет
    mov [rdi+8], ax
    mov al, 'K'
    mov [rdi+10], ax
    jmp .halt
    
.halt:
    cli
    hlt
    jmp $

[BITS 16]
; ===== ФУНКЦИИ 16-BIT РЕЖИМА =====

print_string_16:
    push ax
    push bx
    push si
    
    mov ah, 0x0E
    mov bh, 0
    
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
    
.done:
    pop si
    pop bx
    pop ax
    ret

wait_key:
    push ax
    mov ah, 0x00
    int 0x16
    pop ax
    ret

; Улучшенная функция включения A20
enable_a20_enhanced:
    push ax
    push cx
    
    ; Сначала проверим, не включена ли A20 уже
    call test_a20
    jnc .a20_done
    
    ; Метод 1: BIOS function
    mov ax, 0x2401
    int 0x15
    call test_a20
    jnc .a20_done
    
    ; Метод 2: Keyboard controller
    call a20_wait
    mov al, 0xAD        ; Disable keyboard
    out 0x64, al
    
    call a20_wait
    mov al, 0xD0        ; Read output port
    out 0x64, al
    
    call a20_wait2
    in al, 0x60
    push ax
    
    call a20_wait
    mov al, 0xD1        ; Write output port
    out 0x64, al
    
    call a20_wait
    pop ax
    or al, 2            ; Set A20 bit
    out 0x60, al
    
    call a20_wait
    mov al, 0xAE        ; Enable keyboard
    out 0x64, al
    
    call a20_wait
    call test_a20
    jnc .a20_done
    
    ; Метод 3: Fast A20 (port 0x92)
    in al, 0x92
    test al, 2
    jnz .a20_done
    or al, 2
    and al, 0xFE
    out 0x92, al
    
.a20_done:
    pop cx
    pop ax
    
    mov si, msg_a20_enabled
    call print_string_16
    ret

; Тест A20 линии
test_a20:
    push ax
    push bx
    push es
    push ds
    
    ; Устанавливаем сегменты для теста
    xor ax, ax
    mov es, ax
    mov ds, ax
    
    mov bx, 0x7DFE      ; Адрес в первом мегабайте
    mov al, [es:bx]     ; Сохраняем оригинальное значение
    push ax
    
    mov ax, 0xFFFF
    mov es, ax
    mov bx, 0x7E0E      ; Соответствующий адрес во втором мегабайте
    mov ah, [es:bx]     ; Сохраняем оригинальное значение
    push ax
    
    ; Записываем тестовые значения
    mov byte [es:bx], 0x00
    xor ax, ax
    mov es, ax
    mov byte [es:0x7DFE], 0xFF
    
    ; Проверяем
    mov ax, 0xFFFF
    mov es, ax
    cmp byte [es:bx], 0xFF
    
    ; Восстанавливаем значения
    pop ax
    mov [es:bx], ah
    xor ax, ax
    mov es, ax
    pop ax
    mov [es:0x7DFE], al
    
    pop ds
    pop es
    pop bx
    pop ax
    
    ; CF=0 если A20 включена, CF=1 если выключена
    je .a20_disabled
    clc
    ret
.a20_disabled:
    stc
    ret

a20_wait:
    in al, 0x64
    test al, 2
    jnz a20_wait
    ret

a20_wait2:
    in al, 0x64
    test al, 1
    jz a20_wait2
    ret

load_kernel_simple:
    mov si, msg_loading_kernel
    call print_string_16

    ; Проверка поддержки INT 13h Extensions
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, 0x80
    int 0x13
    jc .use_chs          ; Если не поддерживается, используем CHS

    ; Используем INT 13h Extensions (LBA)
    ; Загружаем 200 секторов (100KB) начиная с LBA 10

    ; Часть 1: 127 секторов
    mov si, dap1
    mov ah, 0x42
    mov dl, 0x80
    int 0x13
    jc .disk_error

    ; Часть 2: 73 сектора
    mov si, dap2
    mov ah, 0x42
    mov dl, 0x80
    int 0x13
    jc .disk_error
    jmp .check_kernel

.use_chs:
    ; Загружаем меньшими порциями, не переходя границу дорожки
    ; Часть 1: 53 сектора (сектора 11-63 на головке 0) → 0x10000
    mov ah, 0x02
    mov al, 53
    mov ch, 0
    mov cl, 11
    mov dh, 0
    mov dl, 0x80
    mov bx, 0x1000
    mov es, bx
    mov bx, 0x0000
    int 0x13
    jc .disk_error

    ; Часть 2: 63 сектора (вся головка 1) → 0x16A00
    mov ah, 0x02
    mov al, 63
    mov ch, 0
    mov cl, 1
    mov dh, 1
    mov dl, 0x80
    mov bx, 0x16A0
    mov es, bx
    mov bx, 0x0000
    int 0x13
    jc .disk_error

    ; Часть 3: 63 сектора (вся головка 2) → 0x1E800
    mov ah, 0x02
    mov al, 63
    mov ch, 0
    mov cl, 1
    mov dh, 2
    mov dl, 0x80
    mov bx, 0x1E80
    mov es, bx
    mov bx, 0x0000
    int 0x13
    jc .disk_error

    ; Часть 4: 21 сектор (головка 3) → 0x26600
    mov ah, 0x02
    mov al, 21
    mov ch, 0
    mov cl, 1
    mov dh, 3
    mov dl, 0x80
    mov bx, 0x2660
    mov es, bx
    mov bx, 0x0000
    int 0x13
    jc .disk_error

.check_kernel:
    
    ; Проверка загрузки (проверяем первые 4 байта)
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x0000
    mov eax, [es:bx]
    test eax, eax
    jz .empty_kernel
    
    ; Восстановка ES
    xor ax, ax
    mov es, ax
    
    mov si, msg_kernel_loaded
    call print_string_16
    ret
    
.empty_kernel:
    xor ax, ax
    mov es, ax
    mov si, msg_kernel_empty
    call print_string_16
    ret
    
.disk_error:
    mov si, msg_disk_error
    call print_string_16
    call wait_key
    cli
    hlt

detect_memory_e820:
    mov si, msg_detecting_memory
    call print_string_16
    
    ; Очистим e820 буфер
    mov ax, 0x9000
    mov es, ax
    xor di, di
    mov cx, 1024        ; Увеличиваем буфер
    xor ax, ax
    rep stosw
    
    ; E820 detection
    xor ebx, ebx
    mov edx, 0x534D4150    ; 'SMAP'
    mov ax, 0x9000
    mov es, ax
    xor di, di
    xor bp, bp             ; Счетчик записей

.e820_loop:
    mov eax, 0xE820
    mov ecx, 24
    mov edx, 0x534D4150
    int 0x15
    jc .e820_fail
    
    ; Проверяем подпись
    cmp eax, 0x534D4150
    jne .e820_fail
    
    ; Проверяем размер записи
    cmp ecx, 20
    jl .skip_entry
    
    ; Увеличиваем счетчик и указатель
    inc bp
    add di, 24
    
.skip_entry:
    ; Проверяем, есть ли еще записи
    test ebx, ebx
    jnz .e820_loop
    
    ; Сохраняем количество записей
    mov [0x8FFE], bp
    shl bp, 4               ; Умножаем на 24 (приблизительно)
    add bp, bp
    add bp, bp
    add bp, bp
    mov [0x8FFC], bp        ; Размер в байтах
    
    mov si, msg_e820_success
    call print_string_16
    ret

.e820_fail:
    mov si, msg_e820_fail
    call print_string_16
    
    ; Fallback: создаем минимальную карту памяти
    mov ax, 0x9000
    mov es, ax
    xor di, di
    
    ; Первая запись: 0-640KB (usable)
    mov dword [es:di], 0x00000000
    mov dword [es:di+4], 0x00000000
    mov dword [es:di+8], 0x0009FC00    ; 640KB
    mov dword [es:di+12], 0x00000000
    mov dword [es:di+16], 1            ; Usable
    mov dword [es:di+20], 0
    add di, 24
    
    ; Вторая запись: 1MB+ (usable, получаем размер через int 15h)
    mov ah, 0x88
    int 0x15
    jc .memory_fail
    
    mov dword [es:di], 0x00100000      ; 1MB
    mov dword [es:di+4], 0x00000000
    movzx eax, ax
    shl eax, 10                        ; KB to bytes
    mov [es:di+8], eax
    mov dword [es:di+12], 0x00000000
    mov dword [es:di+16], 1            ; Usable
    mov dword [es:di+20], 0
    
    ; Сохраняем размер (2 записи)
    mov word [0x8FFE], 2
    mov word [0x8FFC], 48
    
    mov si, msg_memory_fallback
    call print_string_16
    ret

.memory_fail:
    mov si, msg_memory_error
    call print_string_16
    ret

[BITS 32]
; ===== ФУНКЦИИ 32-BIT РЕЖИМА =====

setup_paging_fixed:
    ; Очистка области для таблиц страниц (16KB)
    mov edi, 0x70000
    mov ecx, 4096          ; 16KB / 4 = 4096 dwords
    xor eax, eax
    rep stosd
    
    ; PML4 Table (0x70000) - только первая запись
    mov dword [0x70000], 0x71003      ; Present, Writable, User
    
    ; PDPT (0x71000) - только первая запись
    mov dword [0x71000], 0x72003      ; Present, Writable, User
    
    ; PD (0x72000) - маппинг первых 16MB как 2MB страницы
    mov edi, 0x72000
    mov eax, 0x000083     ; Present, Writable, Page Size (2MB)
    mov ecx, 8            ; 8 записей по 2MB = 16MB
    
.fill_pd:
    mov [edi], eax
    add eax, 0x200000     ; Следующие 2MB
    add edi, 8
    loop .fill_pd
    
    ret

enable_long_mode_fixed:
    ; Включение PAE в CR4
    mov eax, cr4
    or eax, (1 << 5)      ; PAE bit
    mov cr4, eax
    
    ; Загрузка PML4 в CR3
    mov eax, 0x70000
    mov cr3, eax
    
    ; Включение Long Mode в EFER
    mov ecx, 0xC0000080   ; EFER MSR
    rdmsr
    or eax, (1 << 8)      ; LME bit
    wrmsr
    
    ; Включение paging в CR0
    mov eax, cr0
    or eax, (1 << 31)     ; PG bit
    mov cr0, eax
    
    ret

; ===== ИСПРАВЛЕННЫЙ GDT =====
align 8
gdt_start:
    ; 0x00: Null Descriptor
    dq 0x0000000000000000

    ; 0x08: 32-bit Kernel Code Segment
    dw 0xFFFF       ; Limit 15:0
    dw 0x0000       ; Base 15:0
    db 0x00         ; Base 23:16
    db 0x9A         ; Access: Present, Ring 0, Code, Executable, Readable
    db 0xCF         ; Flags: 4KB granularity, 32-bit, Limit 19:16 = 0xF
    db 0x00         ; Base 31:24

    ; 0x10: 32-bit Kernel Data Segment
    dw 0xFFFF       ; Limit 15:0
    dw 0x0000       ; Base 15:0
    db 0x00         ; Base 23:16
    db 0x92         ; Access: Present, Ring 0, Data, Writable
    db 0xCF         ; Flags: 4KB granularity, 32-bit, Limit 19:16 = 0xF
    db 0x00         ; Base 31:24

    ; 0x18: 64-bit Kernel Code Segment
    dw 0x0000       ; Limit (ignored in 64-bit)
    dw 0x0000       ; Base (ignored in 64-bit)
    db 0x00         ; Base (ignored in 64-bit)
    db 0x9A         ; Access: Present, Ring 0, Code, Executable, Readable
    db 0x20         ; Flags: Long mode bit (L=1), все остальные 0
    db 0x00         ; Base (ignored in 64-bit)

    ; 0x20: 64-bit Kernel Data Segment
    dw 0x0000       ; Limit (ignored in 64-bit)
    dw 0x0000       ; Base (ignored in 64-bit)
    db 0x00         ; Base (ignored in 64-bit)
    db 0x92         ; Access: Present, Ring 0, Data, Writable
    db 0x00         ; Flags: (все биты 0 для data segment)
    db 0x00         ; Base (ignored in 64-bit)

gdt_end:

align 4
gdt_descriptor:
    dw gdt_end - gdt_start - 1    ; Limit
    dd gdt_start                  ; Base address (32-bit в 16-bit режиме)

; ===== DAP для INT 13h Extensions (LBA) =====
align 4
dap1:
    db 0x10             ; Размер DAP (16 байт)
    db 0                ; Зарезервировано
    dw 127              ; Количество секторов (127)
    dw 0x0000           ; Offset
    dw 0x1000           ; Segment (0x1000:0x0000 = 0x10000)
    dq 10               ; LBA начальный сектор (10)

align 4
dap2:
    db 0x10             ; Размер DAP (16 байт)
    db 0                ; Зарезервировано
    dw 93               ; Количество секторов (73)
    dw 0x0000           ; Offset
    dw 0x1FE0           ; Segment (0x1FE0:0x0000 = 0x1FE00)
    dq 137              ; LBA начальный сектор (10 + 127 = 137)

; ===== СООБЩЕНИЯ =====
msg_stage2_start      db 'Boxloader Stage2 Started - Press any key', 13, 10, 0
msg_a20_enabled       db 'A20 line enabled', 13, 10, 0
msg_detecting_memory  db 'Detecting memory...', 13, 10, 0
msg_e820_success      db 'E820 memory detection successful', 13, 10, 0
msg_e820_fail         db 'E820 failed, using fallback', 13, 10, 0
msg_memory_fallback   db 'Using INT 15h AH=88h for memory', 13, 10, 0
msg_memory_error      db 'Memory detection failed!', 13, 10, 0
msg_loading_kernel    db 'Loading kernel...', 13, 10, 0
msg_kernel_loaded     db 'Kernel loaded successfully', 13, 10, 0
msg_kernel_empty      db 'Warning: Kernel appears empty', 13, 10, 0
msg_disk_error        db 'Disk read error!', 13, 10, 0
msg_entering_protected db 'Entering protected mode - Press any key', 13, 10, 0


; Заполнение до 4KB
times 4096-($-$$) db 0