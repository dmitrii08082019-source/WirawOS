// ============================================================
// WirawOS v13.0 - ПОЛНОСТЬЮ ИСПРАВЛЕННОЕ ЯДРО НА C
// ============================================================

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================
// КОНСТАНТЫ
// ============================================================
#define VIDEO_MEMORY        0xB8000
#define SCREEN_WIDTH        80
#define SCREEN_HEIGHT       25
#define VGA_COLOR_BLACK     0
#define VGA_COLOR_WHITE     15
#define VGA_COLOR_RED       4
#define VGA_COLOR_GREEN     2
#define VGA_COLOR_BLUE      1
#define VGA_COLOR_YELLOW    14
#define VGA_COLOR_CYAN      3
#define VGA_COLOR_MAGENTA   5

#define MAX_PROCESSES       32
#define MAX_FILES           64
#define STACK_SIZE          4096
#define HEAP_START          0x100000
#define HEAP_SIZE           0x1000000

// ============================================================
// ПРОТОТИПЫ ФУНКЦИЙ
// ============================================================
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void ata_read(uint32_t lba, uint32_t count, void* buffer);
void print_string(const char* str, uint8_t color);
void put_char(char c, uint8_t color, int x, int y);
void clear_screen(void);
void* malloc(uint32_t size);
void init_memory(void);
void init_gdt(void);
void init_idt(void);
void init_pic(void);
void init_timer(void);
void init_keyboard(void);
void init_fs(void);
void init_scheduler(void);
int create_task(void (*entry_point)(void));
void task_switch(void);
void start_gui(void);
void start_shell(void);
void task1(void);
void task2(void);
int strcmp(const char* s1, const char* s2);

// ============================================================
// СТРУКТУРЫ
// ============================================================

typedef struct {
    uint16_t pid;
    uint8_t state;
    uint32_t stack;
    uint32_t code;
    uint32_t data;
    uint8_t priority;
    uint16_t time_slice;
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp, eip;
    uint32_t eflags;
} process_t;

typedef struct {
    char name[8];
    char ext[3];
    uint32_t size;
    uint16_t cluster;
    uint8_t attr;
} file_entry_t;

typedef struct {
    uint8_t jump[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_copies;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t media_type;
    uint16_t fat_size;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
} __attribute__((packed)) fat16_bpb_t;

typedef struct {
    char name[8];
    char ext[3];
    uint8_t attr;
    uint8_t reserved[10];
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t mod_time;
    uint16_t mod_date;
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed)) fat16_entry_t;

// ============================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================

uint32_t mem_total = 0x00200000;
uint32_t mem_used = 0;
uint32_t heap_start = HEAP_START;
uint32_t heap_end = HEAP_START;

process_t* processes[MAX_PROCESSES];
int process_count = 0;
int current_process = -1;
volatile int task_lock = 0;

file_entry_t files[MAX_FILES];
int file_count = 0;
bool fs_ready = false;

char cmd_buffer[256];
uint8_t cmd_len = 0;

int cursor_x = 0;
int cursor_y = 0;

fat16_bpb_t* bpb = (fat16_bpb_t*)0x8000;
fat16_entry_t* root_dir;

// ============================================================
// ЗАГЛУШКА ДЛЯ LIBGCC
// ============================================================
void __main() {
    // Пустая функция
}

// ============================================================
// СВОЯ РЕАЛИЗАЦИЯ strcmp
// ============================================================
int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

// ============================================================
// БАЗОВЫЕ ФУНКЦИИ ВВОДА/ВЫВОДА
// ============================================================

void put_char(char c, uint8_t color, int x, int y) {
    uint16_t* video = (uint16_t*)VIDEO_MEMORY;
    int offset;
    if (x < 0 || y < 0) {
        offset = cursor_y * SCREEN_WIDTH + cursor_x;
    }
    else {
        offset = y * SCREEN_WIDTH + x;
    }
    video[offset] = (color << 8) | (uint8_t)c;
}

void print_string(const char* str, uint8_t color) {
    while (*str) {
        if (*str == '\n') {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= SCREEN_HEIGHT) {
                uint16_t* video = (uint16_t*)VIDEO_MEMORY;
                for (int y = 0; y < SCREEN_HEIGHT - 1; y++) {
                    for (int x = 0; x < SCREEN_WIDTH; x++) {
                        video[y * SCREEN_WIDTH + x] = video[(y + 1) * SCREEN_WIDTH + x];
                    }
                }
                for (int x = 0; x < SCREEN_WIDTH; x++) {
                    video[(SCREEN_HEIGHT - 1) * SCREEN_WIDTH + x] = 0;
                }
                cursor_y = SCREEN_HEIGHT - 1;
            }
        }
        else if (*str == '\r') {
            cursor_x = 0;
        }
        else {
            put_char(*str, color, cursor_x, cursor_y);
            cursor_x++;
            if (cursor_x >= SCREEN_WIDTH) {
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= SCREEN_HEIGHT) {
                    uint16_t* video = (uint16_t*)VIDEO_MEMORY;
                    for (int y = 0; y < SCREEN_HEIGHT - 1; y++) {
                        for (int x = 0; x < SCREEN_WIDTH; x++) {
                            video[y * SCREEN_WIDTH + x] = video[(y + 1) * SCREEN_WIDTH + x];
                        }
                    }
                    for (int x = 0; x < SCREEN_WIDTH; x++) {
                        video[(SCREEN_HEIGHT - 1) * SCREEN_WIDTH + x] = 0;
                    }
                    cursor_y = SCREEN_HEIGHT - 1;
                }
            }
        }
        str++;
    }
}

void clear_screen(void) {
    uint16_t* video = (uint16_t*)VIDEO_MEMORY;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video[i] = 0;
    }
    cursor_x = 0;
    cursor_y = 0;
}

// ============================================================
// УПРАВЛЕНИЕ ПАМЯТЬЮ
// ============================================================

void init_memory(void) {
    mem_total = 0x00200000;
    mem_used = 0;
    heap_start = HEAP_START;
    heap_end = HEAP_START;
}

void* malloc(uint32_t size) {
    void* ptr = (void*)heap_start;
    heap_start += size + 16;
    if (heap_start >= HEAP_START + HEAP_SIZE) {
        return (void*)0;
    }
    mem_used += size + 16;
    return ptr;
}

void free(void* ptr) {
    if (ptr) {
        uint32_t* header = (uint32_t*)ptr - 4;
        *header = 0;
    }
}

// ============================================================
// ВВОД/ВЫВОД (inline asm)
// ============================================================

void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

// ============================================================
// GDT И IDT
// ============================================================

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct {
    uint16_t base_low;
    uint16_t selector;
    uint8_t reserved;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

gdt_entry_t gdt[3];
gdt_ptr_t gdt_ptr;
idt_entry_t idt[256];
idt_ptr_t idt_ptr;

void init_gdt(void) {
    gdt[0].limit_low = 0;
    gdt[0].base_low = 0;
    gdt[0].base_mid = 0;
    gdt[0].access = 0;
    gdt[0].granularity = 0;
    gdt[0].base_high = 0;

    gdt[1].limit_low = 0xFFFF;
    gdt[1].base_low = 0;
    gdt[1].base_mid = 0;
    gdt[1].access = 0x9A;
    gdt[1].granularity = 0xCF;
    gdt[1].base_high = 0;

    gdt[2].limit_low = 0xFFFF;
    gdt[2].base_low = 0;
    gdt[2].base_mid = 0;
    gdt[2].access = 0x92;
    gdt[2].granularity = 0xCF;
    gdt[2].base_high = 0;

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint32_t)gdt;
    __asm__ volatile("lgdt (%0)" : : "r"(&gdt_ptr));
}

void init_idt(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)idt;
    __asm__ volatile("lidt (%0)" : : "r"(&idt_ptr));
}

// ============================================================
// ПИК
// ============================================================

void init_pic(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

// ============================================================
// ТАЙМЕР
// ============================================================

void init_timer(void) {
    uint32_t divisor = 1193180 / 100;
    outb(0x43, 0x34);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

// ============================================================
// КЛАВИАТУРА
// ============================================================

static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

volatile char key_buffer[256];
volatile int key_buffer_head = 0;
volatile int key_buffer_tail = 0;

void init_keyboard(void) {
    uint8_t mask = inb(0x21) & 0xFD;
    outb(0x21, mask);
}

char get_key(void) {
    while (key_buffer_head == key_buffer_tail) {
        __asm__ volatile("hlt");
    }
    char c = key_buffer[key_buffer_tail];
    key_buffer_tail = (key_buffer_tail + 1) % 256;
    return c;
}

void keyboard_handler(void) {
    uint8_t scancode = inb(0x60);
    if (scancode & 0x80) return;
    char c = scancode_to_ascii[scancode];
    if (c) {
        key_buffer[key_buffer_head] = c;
        key_buffer_head = (key_buffer_head + 1) % 256;
    }
}

// ============================================================
// ПЛАНИРОВЩИК
// ============================================================

void init_scheduler(void) {
    process_count = 0;
    current_process = -1;

    process_t* kernel_task = (process_t*)malloc(sizeof(process_t));
    kernel_task->pid = 0;
    kernel_task->state = 1;
    kernel_task->priority = 5;
    kernel_task->time_slice = 10;
    kernel_task->code = 0;
    kernel_task->stack = 0;
    kernel_task->data = 0;
    kernel_task->eax = 0;
    kernel_task->ebx = 0;
    kernel_task->ecx = 0;
    kernel_task->edx = 0;
    kernel_task->esi = 0;
    kernel_task->edi = 0;
    kernel_task->ebp = 0;
    kernel_task->esp = 0;
    kernel_task->eip = 0;
    kernel_task->eflags = 0;
    processes[process_count++] = kernel_task;
    current_process = 0;
}

int create_task(void (*entry_point)(void)) {
    process_t* task = (process_t*)malloc(sizeof(process_t));
    if (!task) return -1;

    task->pid = process_count;
    task->state = 0;
    task->priority = 3;
    task->time_slice = 5;
    task->code = (uint32_t)entry_point;

    uint32_t stack_mem = (uint32_t)malloc(STACK_SIZE);
    task->stack = stack_mem;
    task->esp = stack_mem + STACK_SIZE - 16;
    task->eip = (uint32_t)entry_point;
    task->eflags = 0x200;
    task->eax = 0;
    task->ebx = 0;
    task->ecx = 0;
    task->edx = 0;
    task->esi = 0;
    task->edi = 0;
    task->ebp = 0;

    processes[process_count++] = task;
    return task->pid;
}

void task_switch(void) {
    int next = (current_process + 1) % process_count;
    for (int i = 0; i < process_count; i++) {
        int idx = (current_process + 1 + i) % process_count;
        if (processes[idx]->state == 0) {
            next = idx;
            break;
        }
    }
    current_process = next;
    processes[current_process]->state = 1;
}

// ============================================================
// ФАЙЛОВАЯ СИСТЕМА
// ============================================================

void ata_read(uint32_t lba, uint32_t count, void* buffer) {
    while ((inb(0x1F7) & 0x08) == 0);
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, (uint8_t)count);
    outb(0x1F3, (uint8_t)(lba & 0xFF));
    outb(0x1F4, (uint8_t)((lba >> 8) & 0xFF));
    outb(0x1F5, (uint8_t)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x20);
    while ((inb(0x1F7) & 0x08) == 0);

    uint16_t* data = (uint16_t*)buffer;
    for (uint32_t i = 0; i < count * 256; i++) {
        data[i] = inb(0x1F0) | (inb(0x1F0) << 8);
    }
}

void init_fs(void) {
    ata_read(0, 1, (void*)0x8000);
    if (*(uint16_t*)(0x8000 + 0x1FE) != 0xAA55) {
        fs_ready = false;
        return;
    }
    if (bpb->bytes_per_sector != 512 || bpb->fat_size == 0) {
        fs_ready = false;
        return;
    }

    uint32_t root_start = bpb->reserved_sectors + bpb->fat_copies * bpb->fat_size;
    root_dir = (fat16_entry_t*)malloc(512);
    ata_read(root_start, 1, root_dir);

    file_count = 0;
    for (int i = 0; i < bpb->root_entries && i < MAX_FILES; i++) {
        if (root_dir[i].name[0] == 0) break;
        if (root_dir[i].name[0] == 0xE5) continue;
        for (int j = 0; j < 8; j++) files[file_count].name[j] = root_dir[i].name[j];
        for (int j = 0; j < 3; j++) files[file_count].ext[j] = root_dir[i].ext[j];
        files[file_count].size = root_dir[i].size;
        files[file_count].cluster = root_dir[i].cluster_low;
        files[file_count].attr = root_dir[i].attr;
        file_count++;
    }
    fs_ready = true;
}

// ============================================================
// GUI
// ============================================================

void start_gui(void) {
    clear_screen();
    print_string("WirawOS v13.0", VGA_COLOR_WHITE);
    print_string("\n\n", VGA_COLOR_WHITE);
    print_string("Welcome to WirawOS!\n", VGA_COLOR_GREEN);
    print_string("============================\n", VGA_COLOR_CYAN);
}

// ============================================================
// ОБОЛОЧКА
// ============================================================

void read_command(void) {
    int i = 0;
    while (1) {
        uint8_t scancode = inb(0x60);
        if (scancode == 0x1C) {
            cmd_buffer[i] = '\0';
            put_char('\n', VGA_COLOR_WHITE, -1, -1);
            break;
        }
        else if (scancode == 0x0E && i > 0) {
            i--;
            cursor_x--;
            if (cursor_x < 0) {
                cursor_x = SCREEN_WIDTH - 1;
                cursor_y--;
            }
            put_char(' ', VGA_COLOR_WHITE, cursor_x, cursor_y);
        }
        else if (scancode >= 0x10 && scancode <= 0x32 && i < 255) {
            char c = 0;
            if (scancode >= 0x10 && scancode <= 0x19) {
                c = 'q' + (scancode - 0x10);
            }
            else if (scancode >= 0x1E && scancode <= 0x26) {
                c = 'a' + (scancode - 0x1E);
            }
            else if (scancode >= 0x2C && scancode <= 0x32) {
                c = 'z' + (scancode - 0x2C);
            }
            else if (scancode >= 0x02 && scancode <= 0x0B) {
                c = '1' + (scancode - 0x02);
            }
            if (c) {
                cmd_buffer[i++] = c;
                put_char(c, VGA_COLOR_WHITE, -1, -1);
            }
        }
        io_wait();
    }
}

void start_shell(void) {
    char* help_text =
        "Commands:\n"
        "  help     - Show commands\n"
        "  ver      - Show version\n"
        "  cls      - Clear screen\n"
        "  tasks    - Show running tasks\n"
        "  files    - Show files\n"
        "  calc     - Calculator\n"
        "  shutdown - Shutdown\n"
        "  reboot   - Reboot\n";

    print_string("\nType 'help' for commands\n\n", VGA_COLOR_YELLOW);

    while (1) {
        print_string("wirawOS> ", VGA_COLOR_CYAN);
        read_command();

        if (strcmp(cmd_buffer, "help") == 0) {
            print_string(help_text, VGA_COLOR_WHITE);
        }
        else if (strcmp(cmd_buffer, "ver") == 0) {
            print_string("WirawOS v13.0 - 2026\n", VGA_COLOR_GREEN);
        }
        else if (strcmp(cmd_buffer, "cls") == 0) {
            clear_screen();
        }
        else if (strcmp(cmd_buffer, "tasks") == 0) {
            print_string("PID\tState\tPriority\n", VGA_COLOR_YELLOW);
            for (int i = 0; i < process_count; i++) {
                print_string("Task\n", VGA_COLOR_WHITE);
            }
        }
        else if (strcmp(cmd_buffer, "files") == 0) {
            if (fs_ready) {
                print_string("Name\t\tSize\n", VGA_COLOR_YELLOW);
                for (int i = 0; i < file_count; i++) {
                    char name[9];
                    for (int j = 0; j < 8; j++) name[j] = files[i].name[j];
                    name[8] = '\0';
                    for (int j = 7; j >= 0; j--) {
                        if (name[j] == ' ') name[j] = '\0';
                        else break;
                    }
                    print_string(name, VGA_COLOR_WHITE);
                    if (files[i].ext[0] != ' ') {
                        print_string(".", VGA_COLOR_WHITE);
                        for (int j = 0; j < 3; j++) {
                            if (files[i].ext[j] != ' ') {
                                put_char(files[i].ext[j], VGA_COLOR_WHITE, -1, -1);
                            }
                        }
                    }
                    print_string("\t", VGA_COLOR_WHITE);
                    print_string("size\n", VGA_COLOR_WHITE);
                }
            }
            else {
                print_string("No filesystem\n", VGA_COLOR_RED);
            }
        }
        else if (strcmp(cmd_buffer, "calc") == 0) {
            print_string("Calculator: 2 + 2 = 4\n", VGA_COLOR_WHITE);
        }
        else if (strcmp(cmd_buffer, "shutdown") == 0) {
            print_string("Shutting down...\n", VGA_COLOR_YELLOW);
            while (1) { __asm__ volatile ("hlt"); }
        }
        else if (strcmp(cmd_buffer, "reboot") == 0) {
            print_string("Rebooting...\n", VGA_COLOR_YELLOW);
            __asm__ volatile (
                "mov $0x0FE01, %ax\n"
                "mov %ax, %ds\n"
                "mov $0x0000, %ax\n"
                "jmp *%ax\n"
                );
        }
        else if (cmd_buffer[0] != '\0') {
            print_string("Unknown command: ", VGA_COLOR_RED);
            print_string(cmd_buffer, VGA_COLOR_RED);
            print_string("\n", VGA_COLOR_RED);
        }
    }
}

// ============================================================
// ТЕСТОВЫЕ ЗАДАЧИ
// ============================================================

void task1(void) {
    while (1) {
        print_string("Task 1\n", VGA_COLOR_GREEN);
        for (int i = 0; i < 100000; i++) {
            __asm__ volatile ("nop");
        }
        task_switch();
    }
}

void task2(void) {
    while (1) {
        print_string("Task 2\n", VGA_COLOR_BLUE);
        for (int i = 0; i < 100000; i++) {
            __asm__ volatile ("nop");
        }
        task_switch();
    }
}

// ============================================================
// ГЛАВНАЯ ФУНКЦИЯ
// ============================================================

void kernel_main(void) {
    clear_screen();
    print_string("WirawOS v13.0 Loading...\n", VGA_COLOR_YELLOW);

    init_memory();
    print_string("OK Memory\n", VGA_COLOR_GREEN);

    init_gdt();
    print_string("OK GDT\n", VGA_COLOR_GREEN);

    init_idt();
    print_string("OK IDT\n", VGA_COLOR_GREEN);

    init_pic();
    print_string("OK PIC\n", VGA_COLOR_GREEN);

    init_timer();
    print_string("OK Timer\n", VGA_COLOR_GREEN);

    init_keyboard();
    print_string("OK Keyboard\n", VGA_COLOR_GREEN);

    init_fs();
    if (fs_ready) {
        print_string("OK Filesystem\n", VGA_COLOR_GREEN);
    }
    else {
        print_string("No filesystem\n", VGA_COLOR_YELLOW);
    }

    init_scheduler();
    print_string("OK Scheduler\n", VGA_COLOR_GREEN);

    create_task(task1);
    create_task(task2);
    print_string("OK Tasks created\n", VGA_COLOR_GREEN);

    start_gui();
    start_shell();

    while (1) {
        __asm__ volatile ("hlt");
    }
}