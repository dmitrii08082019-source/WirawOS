BITS 16
ORG 0x7C00

start:
    cli
    mov ax, 0x0000
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Включение A20
    in al, 0x92
    or al, 0x02
    out 0x92, al

    ; Загрузка GDT
    lgdt [gdt_ptr]

    ; Переход в защищённый режим
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax
    jmp 0x08:protected_mode

BITS 32
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Вызов ядра по адресу 0x100000
    call 0x100000

    ; Бесконечный цикл
    cli
    hlt
    jmp $

gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55