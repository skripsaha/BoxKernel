#include "shell.h"
#include "klib.h"
#include "keyboard.h"
#include "vga.h"
#include "tagfs.h"
#include "task.h"
#include "cpu.h"
#include "pmm.h"
#include "vmm.h"

// ============================================================================
// SHELL STATE
// ============================================================================

static char input_buffer[SHELL_INPUT_BUFFER_SIZE];
static uint32_t input_pos = 0;

// ============================================================================
// COMMAND TABLE
// ============================================================================

static shell_command_t commands[] = {
    {"help", "Show available commands", cmd_help},
    {"clear", "Clear the screen", cmd_clear},
    {"echo", "Print text to console", cmd_echo},
    {"ls", "List files (tag-based)", cmd_ls},
    {"cat", "Display file contents", cmd_cat},
    {"touch", "Create file with tags", cmd_touch},
    {"rm", "Remove file by name", cmd_rm},
    {"ps", "List running tasks", cmd_ps},
    {"info", "Show system information", cmd_info},
    {"reboot", "Reboot the system", cmd_reboot},
    {NULL, NULL, NULL}  // Sentinel
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static void shell_print_prompt(void) {
    kprintf("%[H]%s%[D]", SHELL_PROMPT);
}

static int shell_parse_command(char* input, char** argv) {
    int argc = 0;
    int in_token = 0;

    for (int i = 0; input[i] != '\0' && argc < SHELL_MAX_ARGS; i++) {
        if (input[i] == ' ' || input[i] == '\t') {
            input[i] = '\0';
            in_token = 0;
        } else if (!in_token) {
            argv[argc++] = &input[i];
            in_token = 1;
        }
    }

    return argc;
}

// ============================================================================
// SHELL INITIALIZATION
// ============================================================================

void shell_init(void) {
    keyboard_init();
    input_pos = 0;

    kprintf("\n");
    kprintf("%[S]╔══════════════════════════════════════════════════════╗%[D]\n");
    kprintf("%[S]║          Welcome to BoxOS Shell v1.0                ║%[D]\n");
    kprintf("%[S]╚══════════════════════════════════════════════════════╝%[D]\n");
    kprintf("\n");
    kprintf("Type '%[H]help%[D]' for available commands.\n");
    kprintf("\n");
}

// ============================================================================
// SHELL MAIN LOOP
// ============================================================================

void shell_run(void) {
    shell_print_prompt();

    while (1) {
        if (!keyboard_has_input()) {
            asm("hlt");  // Wait for interrupt
            continue;
        }

        char c = keyboard_getchar();

        if (c == '\n') {
            // Execute command
            kprintf("\n");

            if (input_pos > 0) {
                input_buffer[input_pos] = '\0';

                // Parse command
                char* argv[SHELL_MAX_ARGS];
                int argc = shell_parse_command(input_buffer, argv);

                if (argc > 0) {
                    // Find and execute command
                    int found = 0;
                    for (int i = 0; commands[i].name != NULL; i++) {
                        if (strcmp(argv[0], commands[i].name) == 0) {
                            commands[i].handler(argc, argv);
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        kprintf("%[E]Error: Unknown command '%s'%[D]\n", argv[0]);
                        kprintf("Type '%[H]help%[D]' for available commands.\n");
                    }
                }

                input_pos = 0;
            }

            shell_print_prompt();
        } else if (c == '\b') {
            // Backspace
            if (input_pos > 0) {
                input_pos--;
                kprintf("\b \b");  // Erase character on screen
            }
        } else if (input_pos < SHELL_INPUT_BUFFER_SIZE - 1) {
            // Add character to buffer
            input_buffer[input_pos++] = c;
            kprintf("%c", c);
        }
    }
}

// ============================================================================
// COMMAND: help
// ============================================================================

int cmd_help(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]Available Commands:%[D]\n");
    kprintf("═══════════════════════════════════════════════════\n");

    for (int i = 0; commands[i].name != NULL; i++) {
        kprintf("  %[H]%-12s%[D] - %s\n", commands[i].name, commands[i].description);
    }

    kprintf("\n");
    return 0;
}

// ============================================================================
// COMMAND: clear
// ============================================================================

int cmd_clear(int argc, char** argv) {
    (void)argc;
    (void)argv;

    vga_clear_screen();
    return 0;
}

// ============================================================================
// COMMAND: echo
// ============================================================================

int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s", argv[i]);
        if (i < argc - 1) kprintf(" ");
    }
    kprintf("\n");
    return 0;
}

// ============================================================================
// COMMAND: ls
// ============================================================================

int cmd_ls(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]Filesystem Contents (Tag-Based):%[D]\n");
    kprintf("════════════════════════════════════════════════════════════════════\n");
    kprintf("%-8s %-20s %-10s %s\n", "Inode", "Name", "Size", "Tags");
    kprintf("────────────────────────────────────────────────────────────────────\n");

    // Access inode table directly (we're in kernel mode)
    extern TagFSContext global_tagfs;
    uint32_t file_count = 0;

    for (uint64_t i = 1; i < global_tagfs.superblock->total_inodes; i++) {
        FileInode* inode = &global_tagfs.inode_table[i];

        // Skip empty inodes
        if (inode->inode_id == 0 || inode->size == 0xFFFFFFFFFFFFFFFF) {
            continue;
        }

        // Get filename from name tag
        char filename[TAGFS_TAG_VALUE_SIZE] = "<unnamed>";
        for (uint32_t t = 0; t < inode->tag_count; t++) {
            if (strcmp(inode->tags[t].key, "name") == 0) {
                strncpy(filename, inode->tags[t].value, TAGFS_TAG_VALUE_SIZE);
                break;
            }
        }

        // Print file info
        kprintf("%-8lu %-20s %-10lu ", inode->inode_id, filename, inode->size);

        // Print tags
        kprintf("[");
        for (uint32_t t = 0; t < inode->tag_count && t < 3; t++) {
            if (strcmp(inode->tags[t].key, "name") != 0) {  // Skip name tag
                kprintf("%s:%s", inode->tags[t].key, inode->tags[t].value);
                if (t < inode->tag_count - 1) kprintf(", ");
            }
        }
        if (inode->tag_count > 3) kprintf("...");
        kprintf("]\n");

        file_count++;
    }

    if (file_count == 0) {
        kprintf("(no files)\n");
    }

    kprintf("\nTotal files: %u\n\n", file_count);
    return 0;
}

// ============================================================================
// COMMAND: cat
// ============================================================================

int cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        kprintf("%[E]Usage: cat <filename>%[D]\n");
        return 1;
    }

    // Find file by name tag
    Tag name_tag;
    strncpy(name_tag.key, "name", TAGFS_TAG_KEY_SIZE);
    strncpy(name_tag.value, argv[1], TAGFS_TAG_VALUE_SIZE);

    uint64_t inode_ids[16];
    uint32_t count = 0;
    int result = tagfs_query_single(&name_tag, inode_ids, &count, 16);

    if (result < 0 || count == 0) {
        kprintf("%[E]File not found: %s%[D]\n", argv[1]);
        return 1;
    }

    uint64_t inode_id = inode_ids[0];
    FileInode* inode = tagfs_get_inode(inode_id);

    if (!inode) {
        kprintf("%[E]Failed to get inode: %lu%[D]\n", inode_id);
        return 1;
    }

    kprintf("\n%[H]File: %s (inode=%lu, size=%lu bytes)%[D]\n",
            argv[1], inode_id, inode->size);
    kprintf("═══════════════════════════════════════════════════\n");

    if (inode->size == 0) {
        kprintf("(empty file)\n");
    } else {
        // Read file content (max 4KB for display)
        uint8_t buffer[4096];
        uint64_t read_size = (inode->size > 4096) ? 4096 : inode->size;

        result = tagfs_read_file(inode_id, 0, buffer, read_size);

        if (result < 0) {
            kprintf("%[E]Failed to read file%[D]\n");
            return 1;
        }

        // Print content (assuming text)
        for (uint64_t i = 0; i < read_size; i++) {
            kprintf("%c", buffer[i]);
        }

        if (inode->size > 4096) {
            kprintf("\n... (truncated, file is %lu bytes) ...\n", inode->size);
        }
    }

    kprintf("\n");
    return 0;
}

// NOTE: mkdir removed - TagFS uses tags, not directories!

// ============================================================================
// COMMAND: touch
// ============================================================================

int cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        kprintf("%[E]Usage: touch <filename> [tag1:value1] [tag2:value2] ...%[D]\n");
        kprintf("Example: touch myfile.txt type:document format:txt\n");
        return 1;
    }

    // Create tags array (filename + additional tags)
    Tag tags[TAGFS_MAX_TAGS_PER_FILE];
    uint32_t tag_count = 0;

    // First tag: name:filename
    strncpy(tags[tag_count].key, "name", TAGFS_TAG_KEY_SIZE);
    strncpy(tags[tag_count].value, argv[1], TAGFS_TAG_VALUE_SIZE);
    tag_count++;

    // Parse additional tags from command line
    for (int i = 2; i < argc && tag_count < TAGFS_MAX_TAGS_PER_FILE; i++) {
        Tag tag = tagfs_tag_from_string(argv[i]);
        if (tag.key[0] != '\0') {  // Valid tag
            tags[tag_count++] = tag;
        }
    }

    // Create file
    uint64_t inode_id = tagfs_create_file(tags, tag_count);

    if (inode_id == TAGFS_INVALID_INODE) {
        kprintf("%[E]Failed to create file: %s%[D]\n", argv[1]);
        return 1;
    }

    kprintf("%[S]Created file '%s' (inode=%lu) with %u tags%[D]\n",
            argv[1], inode_id, tag_count);
    return 0;
}

// ============================================================================
// COMMAND: rm
// ============================================================================

int cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        kprintf("%[E]Usage: rm <filename>%[D]\n");
        return 1;
    }

    // Find file by name tag
    Tag name_tag;
    strncpy(name_tag.key, "name", TAGFS_TAG_KEY_SIZE);
    strncpy(name_tag.value, argv[1], TAGFS_TAG_VALUE_SIZE);

    uint64_t inode_ids[16];
    uint32_t count = 0;
    int result = tagfs_query_single(&name_tag, inode_ids, &count, 16);

    if (result < 0 || count == 0) {
        kprintf("%[E]File not found: %s%[D]\n", argv[1]);
        return 1;
    }

    // Delete first matching file
    result = tagfs_delete_file(inode_ids[0]);

    if (result < 0) {
        kprintf("%[E]Failed to delete file: %s%[D]\n", argv[1]);
        return 1;
    }

    kprintf("%[S]Removed: %s (inode=%lu)%[D]\n", argv[1], inode_ids[0]);
    return 0;
}

// ============================================================================
// COMMAND: ps
// ============================================================================

int cmd_ps(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]Running Tasks:%[D]\n");
    kprintf("═══════════════════════════════════════════════════════════════════\n");
    kprintf("%-6s %-20s %-12s %-8s %-8s %-8s\n",
            "ID", "Name", "State", "Energy", "Health", "Events");
    kprintf("───────────────────────────────────────────────────────────────────\n");

    // Get all tasks
    Task* tasks[256];
    int count = task_enumerate(tasks, 256);

    if (count == 0) {
        kprintf("No tasks running.\n");
    } else {
        const char* state_names[] = {
            "Idle", "Running", "Waiting", "Sleeping",
            "Blocked", "Paused", "Zombie", "Dead"
        };

        for (int i = 0; i < count; i++) {
            Task* t = tasks[i];
            const char* state_str = (t->state < 8) ? state_names[t->state] : "Unknown";

            kprintf("%-6lu %-20s %-12s %-8u %-8u %-8lu\n",
                    t->task_id,
                    t->name,
                    state_str,
                    t->energy_allocated,
                    t->health,
                    t->events_processed);
        }
    }

    kprintf("\nTotal tasks: %d\n", count);
    kprintf("\n");
    return 0;
}

// ============================================================================
// COMMAND: info
// ============================================================================

int cmd_info(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]═══════════════════════════════════════════════════%[D]\n");
    kprintf("%[H]             BoxOS System Information              %[D]\n");
    kprintf("%[H]═══════════════════════════════════════════════════%[D]\n\n");

    // CPU info
    char cpu_vendor[13];
    char cpu_brand[49];
    detect_cpu_info(cpu_vendor, cpu_brand);

    kprintf("%[H]CPU:%[D]\n");
    kprintf("  Vendor: %s\n", cpu_vendor);
    kprintf("  Brand:  %s\n", cpu_brand);

    // Memory info
    vmm_stats_t stats;
    vmm_get_global_stats(&stats);

    kprintf("\n%[H]Memory:%[D]\n");
    kprintf("  Total mapped: %lu MB\n",
           (stats.total_mapped_pages * 4096) / (1024 * 1024));
    kprintf("  Kernel pages: %lu\n", stats.kernel_mapped_pages);
    kprintf("  User pages:   %lu\n", stats.user_mapped_pages);

    kprintf("\n%[H]Filesystem:%[D]\n");
    kprintf("  Type: TagFS (tag-based filesystem)\n");
    kprintf("  Blocks: 128 (512 KB total)\n");

    kprintf("\n");
    return 0;
}

// ============================================================================
// COMMAND: reboot
// ============================================================================

int cmd_reboot(int argc, char** argv) {
    (void)argc;
    (void)argv;

    kprintf("\n%[H]Rebooting system...%[D]\n\n");

    // Wait a moment
    for (volatile int i = 0; i < 10000000; i++);

    // Triple fault to reboot
    asm volatile("cli");
    asm volatile("lidt (0)");  // Load invalid IDT
    asm volatile("int $3");    // Trigger interrupt

    return 0;
}
