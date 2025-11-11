[BITS 16]
[ORG 0x7C00]

; Stage1 - Первая стадия загрузчика (MBR)
; Загружает Stage2 с диска и передает управление

start_stage1:
    ; Настройка сегментов
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    ; Очистка экрана
    mov ax, 0x0003
    int 0x10
    
    ; Вывод сообщения о загрузке
    mov si, msg_loading
    call print_string
    
    ; Сброс дисковода
    mov ah, 0x00
    mov dl, 0x80
    int 0x13
    jc disk_error
    
    ; Загрузка Stage2 (сектора 2-10, 9 секторов)
    mov ah, 0x02        ; Функция чтения
    mov al, 9           ; Количество секторов
    mov ch, 0           ; Цилиндр 0
    mov cl, 2           ; Начальный сектор (2)
    mov dh, 0           ; Головка 0
    mov dl, 0x80        ; Диск 0x80
    mov bx, 0x8000      ; Адрес загрузки Stage2
    int 0x13
    jc disk_error
    
    ; Проверка подписи Stage2
    mov ax, [0x8000]
    cmp ax, 0x2907
    jne stage2_error
    
    ; Передача управления Stage2
    mov si, msg_stage2
    call print_string
    jmp 0x0000:0x8000

    
disk_error:
    mov si, msg_disk_error
    call print_string
    jmp hang
    
stage2_error:
    mov si, msg_stage2_error
    call print_string
    jmp hang
    
hang:
    hlt
    jmp hang

; Функция вывода строки
print_string:
    push ax
    push bx
    mov ah, 0x0E
    mov bh, 0
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; Сообщения
msg_loading db "Boxloader Stage1 Loading...", 13, 10, 0
msg_stage2 db "Jumping to Stage2...", 13, 10, 0
msg_disk_error db "Disk read error!", 13, 10, 0
msg_stage2_error db "Stage2 signature error!", 13, 10, 0

; Заполнение до 510 байт
times 510-($-$$) db 0

; Boot signature
dw 0xAA55