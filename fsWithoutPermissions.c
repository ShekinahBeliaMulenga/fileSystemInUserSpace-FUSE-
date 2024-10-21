/*
    DEVELOPER: SHKINAH B MULENGA
    PARTNER: GLORIA CHOMBA




    FILE SYSTEM IMPLEMENTATION OVERVIEW

    This code implements and demonstrates the simulation a simple file system in user space with the following features:
    - Block and inode management.
    - File creation, deletion, renaming, and modification.
    - Access Control Lists (ACLs) for managing permissions.
    - A journal for tracking operations to support recovery.
    - Integration with GTK for a graphical user interface.

    Allocation Technique:

    - Direct Allocation: Each file inode contains direct pointers to a fixed number of blocks (DIRECT_BLOCKS). These blocks
      are used for storing file data directly.

    - Indexed Allocation: One additional block is used for indirect addressing (INDEX_BLOCKS). This block can point to more
      blocks if the file size exceeds the capacity of direct blocks.

    Limitations:
    - The file system does not properly restore file permissions during the recovery process because inodes
    and their associated permissions are not preserved on Windows systems.
    This is due to the lack of native support for inodes in the Windows operating system,
    which means that permissions and other inode-based metadata are not correctly handled or restored.

    However, simulation of permission operations can be observed when the system is compiled and run.
    This allows users to experience how permissions would function in a typical Unix-like environment,
    even though the actual restoration of file permissions may not be fully supported on Windows.

    Key Components:
    - Superblock: Contains metadata about the file system.
    - Inodes: Represent files and directories.
    - Data Blocks: Store file data.
    - Journal: Records file system operations for recovery.
    - ACLs: Manage file access permissions for different users.
    - GUI Integration: Provides a GTK-based interface for user interactions.

    Note: This implementation is tailored for Windows environments and some Unix-like features such as inodes may not be fully supported or restored.

    Permissions are set according to the following bit mask:
    - 0400: Owner read
    - 0200: Owner write
    - 0100: Owner execute
    - 0777: all default permissions


*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <dirent.h>
#include<direct.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gio/gio.h>

#define BLOCK_SIZE 4096
#define NUM_BLOCKS 1024
#define INODE_TABLE_SIZE 128
#define MAX_FILENAME_LEN 255
#define MAX_FILES 128
#define DIRECT_BLOCKS 12
#define INDEX_BLOCKS 1
#define MAX_ACL_ENTRIES 10
#define MAX_USERS 100

//permission definitions
#define PERMISSION_READ  0x4
#define PERMISSION_WRITE 0x2
#define PERMISSION_EXECUTE 0x1

#define JOURNAL_SIZE 1024
#define JOURNAL_FILENAME "journal.log"

#define INODE_TABLE_FILENAME "inode_table.bin"

typedef enum {
    CREATE,
    DELETE,
    MODIFY,
    RENAME,
    READ,
    CHANGE_PERMISSIONS
} JournalOperation;

typedef struct {
    JournalOperation operation;
    char filename[MAX_FILENAME_LEN];
    char new_filename[MAX_FILENAME_LEN]; // For rename operation
    char data[BLOCK_SIZE]; // For modify operation
    time_t timestamp;
} JournalEntry;

JournalEntry journal[JOURNAL_SIZE];
int journal_index = 0;

//SuperBlock Structure
typedef struct {
    int _size;
    int num_blocks;
    int free_blocks;
    int inode_table_size;
    int free_inode_count;
    int free_block_bitmap[NUM_BLOCKS];
} Superblock;

//access control list structure
typedef struct {
    int user_id;
    unsigned int permissions;
} ACL_Entry;

//inode structure
typedef struct {
    int is_directory;
    int _size;
    int direct_blocks[DIRECT_BLOCKS];
    int index_block;
    unsigned int mode;
    time_t atime; // access time
    time_t mtime; // modification time
    time_t ctime; // creation time
    int owner_id;
    int group_id;
    ACL_Entry acl[MAX_ACL_ENTRIES];
    int acl_count;
} Inode;

typedef struct {
    char name[MAX_FILENAME_LEN];
    int inode_number;
} DirectoryEntry;

typedef struct {
    DirectoryEntry entries[MAX_FILES];
    int entry_count;
} Directory;

Superblock superblock;
Inode* inode_table[INODE_TABLE_SIZE];
char* data_blocks[NUM_BLOCKS];
Directory root_directory;

static GFileMonitor *monitor;
void init_file_system();
void free_memory();
int allocate_block();
void free_block(int block_number);
int allocate_index_block();
void free_index_block(int block_number);
int create_inode(int is_directory, unsigned int mode, int owner_id, int group_id);
int file_exists(const char *path);
int create_file(const char *path, int is_directory);
void delete_file(const char *filename, GtkWidget *parent);
void rename_file(const char *old_name, const char *new_name, GtkWidget *parent);
void list_files(GtkWidget *widget, gpointer data);
void edit_file(const char *filename, GtkWidget *parent);
void open_file(const char *filename, GtkWidget *parent);
void create_file_dialog(GtkWidget *widget, gpointer data);
void delete_file_dialog(GtkWidget *widget, gpointer data);
void open_file_dialog(GtkWidget *widget, gpointer data);
void show_message_dialog(GtkWidget *parent, GtkMessageType type, const gchar *message);
void set_permissions(int inode_number, unsigned int permissions);
unsigned int get_permissions(int inode_number);
void add_acl_entry(int inode_number, int user_id, unsigned int permissions);
void remove_acl_entry(int inode_number, int user_id);
unsigned int get_acl_permissions(int inode_number, int user_id);
void load_permissions();
void save_permissions();
void save_file_system_state();
void load_file_system_state();

int current_user_id = 11 ; //for testing purposes
int current_group_id = 10 ;
char TEST_FOLDER_PATH[MAX_FILENAME_LEN] = "C:/Users/CLIENT/Music/tests/"; //for current directory, change to your preference for testing
char PREVIOUS_ROOT_PATH[MAX_FILENAME_LEN] = "C:/Users/CLIENT/Music/tests/";//to keep "root directory"

const char* operation_to_string(JournalOperation operation) {
    switch (operation) {
        case CREATE: return "CREATED";
        case DELETE: return "DELETED";
        case MODIFY: return "MODIFIED";
        case RENAME: return "RENAMED";
        case READ: return "READ";
        case CHANGE_PERMISSIONS: return "CHANGED PERMISSIONS";
        default: return "UNKNOWN";
    }
}

JournalOperation string_to_operation(const char* str) {
    if (strcmp(str, "CREATE") == 0) return CREATE;
    if (strcmp(str, "DELETE") == 0) return DELETE;
    if (strcmp(str, "MODIFY") == 0) return MODIFY;
    if (strcmp(str, "RENAME") == 0) return RENAME;
    if (strcmp(str, "READ") == 0) return READ;
    if (strcmp(str, "CHANGE_PERMISSIONS") == 0) return CHANGE_PERMISSIONS;
    return -1; // Unknown operation
}

void init_journal() {
    FILE *file = fopen(JOURNAL_FILENAME, "r");
    if (file) {
        char line[1024];
        int index = 0;
        while (fgets(line, sizeof(line), file) && index < JOURNAL_SIZE) {
            char op_str[20], filename[MAX_FILENAME_LEN], new_filename[MAX_FILENAME_LEN], data[BLOCK_SIZE];
            time_t timestamp;
            if (sscanf(line, "%ld %s %s %s %s", &timestamp, op_str, filename, new_filename, data) >= 3) {
                journal[index].timestamp = timestamp;
                journal[index].operation = string_to_operation(op_str);
                strncpy(journal[index].filename, filename, MAX_FILENAME_LEN - 1);
                strncpy(journal[index].new_filename, new_filename, MAX_FILENAME_LEN - 1);
                strncpy(journal[index].data, data, BLOCK_SIZE - 1);
                index++;
            }
        }
        fclose(file);
        journal_index = index % JOURNAL_SIZE;
    } else {
        printf("Journal file not found, starting fresh\n");
        journal_index = 0;
    }
}


void save_journal() {
    FILE *file = fopen(JOURNAL_FILENAME, "w");
    if (file) {
        for (int i = 0; i < JOURNAL_SIZE; i++) {
            if (journal[i].timestamp == 0) continue; // Skip empty entries
            fprintf(file, "%ld %s %s %s %s\n",
                    journal[i].timestamp,
                    operation_to_string(journal[i].operation),
                    journal[i].filename,
                    journal[i].new_filename,
                    journal[i].data);
        }
        fclose(file);
    } else {
        printf("Error: Could not open journal file for writing\n");
    }
}

void add_journal_entry(JournalOperation operation, const char *filename, const char *new_filename, const char *data) {
    if (filename == NULL) {
        printf("Error: Filename is required for journal entry\n");
        return;
    }

    journal[journal_index].operation = operation;
    strncpy(journal[journal_index].filename, filename, MAX_FILENAME_LEN - 1);
    journal[journal_index].filename[MAX_FILENAME_LEN - 1] = '\0';

    if (new_filename) {
        strncpy(journal[journal_index].new_filename, new_filename, MAX_FILENAME_LEN - 1);
        journal[journal_index].new_filename[MAX_FILENAME_LEN - 1] = '\0';
    } else {
        journal[journal_index].new_filename[0] = '\0';
    }

    if (data) {
        strncpy(journal[journal_index].data, data, BLOCK_SIZE - 1);
        journal[journal_index].data[BLOCK_SIZE - 1] = '\0';
    } else {
        journal[journal_index].data[0] = '\0';
    }

    journal[journal_index].timestamp = time(NULL);
    journal_index = (journal_index + 1) % JOURNAL_SIZE;
    save_journal();
}


void replay_journal() {
    for (int i = 0; i < JOURNAL_SIZE; i++) {
        JournalEntry *entry = &journal[i];
        if (entry->timestamp == 0) continue; // Skip empty entries

        printf("Replaying journal entry: %d, Operation: %s, Filename: %s\n", i, operation_to_string(entry->operation), entry->filename);

        switch (entry->operation) {
            case CREATE:
                if (create_file(entry->filename, 0)) {
                    printf("Error: Failed to create file %s during journal replay\n", entry->filename);
                }
                break;
            case DELETE:
                delete_file(entry->filename, NULL);
                break;
            case MODIFY: {
                FILE *file = fopen(entry->filename, "w");
                if (file) {
                    if (fwrite(entry->data, 1, strlen(entry->data), file) != strlen(entry->data)) {
                        printf("Error: Failed to write data to file %s during journal replay\n", entry->filename);
                    }
                    fclose(file);
                } else {
                    printf("Error: Could not open file %s for writing during journal replay\n", entry->filename);
                }
                break;
            }
            case RENAME:
                rename_file(entry->filename, entry->new_filename, NULL);
                break;
            default:
                printf("Warning: Unknown operation in journal entry\n");
                break;
        }
    }
}


int find_inode_by_filename(const char *filename) {

    if (filename == NULL || strlen(filename) == 0) {
        printf("Error: Filename is NULL.\n");
        return -1;
    }

    printf("Searching for file: '%s'\n", filename);

    for (int i = 0; i < root_directory.entry_count; i++) {

        printf("Comparing with: '%s'\n", root_directory.entries[i].name);

        if (strcmp(root_directory.entries[i].name, filename) == 0) {

            printf("Found inode number %d for file: '%s'\n", root_directory.entries[i].inode_number, filename);
            return root_directory.entries[i].inode_number;
        }
    }

    printf("Error: File '%s' not found in root directory\n", filename);

    return -1;
}



void on_directory_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data) {

    GtkWidget *window = GTK_WIDGET(user_data);
    list_files(NULL, window);
}

void show_message_dialog(GtkWidget *parent, GtkMessageType type, const gchar *message) {

    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_MODAL, type, GTK_BUTTONS_OK, "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}



//file initialization
void init_file_system() {
    // Initialize superblock
    superblock._size = NUM_BLOCKS * BLOCK_SIZE;
    superblock.num_blocks = NUM_BLOCKS;
    superblock.free_blocks = NUM_BLOCKS - 1;
    superblock.inode_table_size = INODE_TABLE_SIZE;
    superblock.free_inode_count = INODE_TABLE_SIZE;
    memset(superblock.free_block_bitmap, 0, sizeof(superblock.free_block_bitmap));

    // Allocate and initialize inodes
    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        inode_table[i] = (Inode*)malloc(sizeof(Inode));
        if (inode_table[i] == NULL) {
            printf("Failed to allocate memory for inode %d\n", i);
            exit(EXIT_FAILURE);
        }
        memset(inode_table[i], 0, sizeof(Inode));
    }

    // Allocate memory for data blocks
    for (int i = 0; i < NUM_BLOCKS; i++) {
        data_blocks[i] = (char *)malloc(BLOCK_SIZE);
        if (data_blocks[i] == NULL) {
            printf("Failed to allocate memory for data block %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    // Initialize root directory
    root_directory.entry_count = 0;

    // Try to load the file system state from disk
    load_file_system_state();
}



void save_file_system_state() {

    FILE *fs_file = fopen("file_system_state.dat", "wb");
    if (!fs_file) {
        perror("Failed to open file system state file");
        return;
    }

    // Save superblock
    fwrite(&superblock, sizeof(superblock), 1, fs_file);

    // Save inodes
    for (int i = 0; i < superblock.inode_table_size; i++) {
        fwrite(inode_table[i], sizeof(Inode), 1, fs_file);
    }

    fclose(fs_file);
}



void load_file_system_state() {

    FILE *fs_file = fopen("file_system_state.dat", "rb");
    if (!fs_file) {
        printf("Initializing new file system.\n");
        return;
    }

    // Load superblock
    fread(&superblock, sizeof(superblock), 1, fs_file);

    // Allocate and load inodes
    for (int i = 0; i < superblock.inode_table_size; i++) {
        inode_table[i] = (Inode*)malloc(sizeof(Inode));
        if (inode_table[i] == NULL) {
            printf("Failed to allocate memory for inode %d\n", i);
            exit(EXIT_FAILURE);
        }
        fread(inode_table[i], sizeof(Inode), 1, fs_file);
    }

    // Allocate memory for data blocks
    for (int i = 0; i < NUM_BLOCKS; i++) {
        data_blocks[i] = (char *)malloc(BLOCK_SIZE);
        if (data_blocks[i] == NULL) {
            printf("Failed to allocate memory for data block %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    fclose(fs_file);
}



//set permissions
void set_permissions(int inode_number, unsigned int permissions) {
    if (inode_number < 0 || inode_number >= INODE_TABLE_SIZE) return;
    if ((permissions & ~(PERMISSION_READ | PERMISSION_WRITE | PERMISSION_EXECUTE)) != 0) {
        printf("Invalid permissions value: %u\n", permissions);
        return;
    }
    inode_table[inode_number]->mode = permissions;
    printf("Set permissions for inode %d to %u\n", inode_number, permissions);
}



//get permissions
unsigned int get_permissions(int inode_number) {

    if (inode_number < 0 || inode_number >= INODE_TABLE_SIZE) return 0;
    return inode_table[inode_number]->mode;
}


void add_acl_entry(int inode_number, int user_id, unsigned int permissions) {

    if (inode_number < 0 || inode_number >= INODE_TABLE_SIZE) return;
    Inode *inode = inode_table[inode_number];
    if (inode->acl_count >= MAX_ACL_ENTRIES) {
        printf("ACL entry limit reached\n");
        return;
    }

    if ((permissions & ~(PERMISSION_READ | PERMISSION_WRITE | PERMISSION_EXECUTE)) != 0) {
        printf("Invalid permissions value: %u\n", permissions);
        return;
    }

    inode->acl[inode->acl_count].user_id = user_id;
    inode->acl[inode->acl_count].permissions = permissions;
    inode->acl_count++;
}



void remove_acl_entry(int inode_number, int user_id) {
    if (inode_number < 0 || inode_number >= INODE_TABLE_SIZE) return;

    Inode *inode = inode_table[inode_number];
    int found = 0;

    for (int i = 0; i < inode->acl_count; i++) {
        if (inode->acl[i].user_id == user_id) {
            memmove(&inode->acl[i], &inode->acl[i + 1], (inode->acl_count - i - 1) * sizeof(ACL_Entry));
            inode->acl_count--;
            found = 1;
            printf("Removed ACL entry for inode %d, user %d\n", inode_number, user_id);
            break;
        }
    }

    if (!found) {
        printf("No ACL entry found for inode %d, user %d\n", inode_number, user_id);
    }
}

unsigned int get_acl_permissions(int inode_number, int user_id) {

    if (inode_number < 0 || inode_number >= INODE_TABLE_SIZE) return 0;
    Inode *inode = inode_table[inode_number];

    for (int i = 0; i < inode->acl_count; i++) {
        if (inode->acl[i].user_id == user_id) {
            printf("Found ACL entry for inode %d, user %d: %u\n", inode_number, user_id, inode->acl[i].permissions);
            return inode->acl[i].permissions;
        }
    }
    return 0;
}

int has_permission(int inode_number, int user_id, unsigned int required_permission) {
    if (inode_number < 0 || inode_number >= INODE_TABLE_SIZE) {
        return 0;
    }

    Inode *inode = inode_table[inode_number];

    // Check the base permissions
    unsigned int permissions = 0;
    if (user_id == inode->owner_id) {
        permissions = (inode->mode >> 6) & 0x7; // Owner permissions
    } else if (user_id == inode->group_id) {
        permissions = (inode->mode >> 3) & 0x7; // Group permissions
    } else {
        permissions = inode->mode & 0x7; // Others permissions
    }

    // Check ACL permissions
    permissions |= get_acl_permissions(inode_number, user_id);

    return (permissions & required_permission) == required_permission;
}




void free_memory() {

    for (int i = 0; i < INODE_TABLE_SIZE; i++) {
        if (inode_table[i]) {
            free(inode_table[i]);
            inode_table[i] = NULL;
        }
    }

    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (data_blocks[i]) {
            free(data_blocks[i]);
            data_blocks[i] = NULL;
        }
    }
}



int allocate_block() {

    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (superblock.free_block_bitmap[i] == 0) {
            superblock.free_block_bitmap[i] = 1;
            superblock.free_blocks--;
            return i;
        }
    }
    return -1;
}

void free_block(int block_number) {

    if (block_number < 0 || block_number >= NUM_BLOCKS) return;
    if (superblock.free_block_bitmap[block_number] == 1) {
        superblock.free_block_bitmap[block_number] = 0;
        superblock.free_blocks++;
    }
}

int allocate_index_block() {

    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (superblock.free_block_bitmap[i] == 0) {
            superblock.free_block_bitmap[i] = 1;
            superblock.free_blocks--;
            return i;
        }
    }

    return -1;
}

void free_index_block(int block_number) {

    if (block_number < 0 || block_number >= NUM_BLOCKS) return;
    if (superblock.free_block_bitmap[block_number] == 1) {
        superblock.free_block_bitmap[block_number] = 0;
        superblock.free_blocks++;
    }
}

int create_inode(int is_directory, unsigned int mode, int owner_id, int group_id) {
    if (superblock.free_inode_count == 0) {
        printf("No free inodes available\n");
        return -1;
    }

    for (int i = 0; i < INODE_TABLE_SIZE; i++) {

        if (inode_table[i]->_size == 0) {
            inode_table[i]->is_directory = is_directory;
            inode_table[i]->_size = 0;
            inode_table[i]->mode = mode;
            inode_table[i]->atime = inode_table[i]->mtime = inode_table[i]->ctime = time(NULL);
            inode_table[i]->owner_id = owner_id;
            inode_table[i]->group_id = group_id;
            inode_table[i]->acl_count = 0;
            memset(inode_table[i]->direct_blocks, -1, sizeof(inode_table[i]->direct_blocks));
            inode_table[i]->index_block = -1;
            superblock.free_inode_count--;
            printf("Created inode for %s\n", is_directory ? "directory" : "file");
            return i;
        }
    }
    return -1;
}



int file_exists(const char *path) {

    char full_path[MAX_FILENAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, path);
    return access(full_path, F_OK) == 0;
}

//create file
int create_file(const char *path, int is_directory) {

    if (file_exists(path)) {
        printf("File already exists: %s\n", path);
        show_message_dialog(NULL, GTK_MESSAGE_INFO, "The file already exists");
        return -1;
    }

    char full_path[MAX_FILENAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, path);

    unsigned int default_permissions = 0777; // Default permissions for files
    int inode_number = create_inode(is_directory, default_permissions, current_user_id, current_group_id);
    if (inode_number == -1) {
        printf("Failed to create inode for file\n");
        return -1;
    }

    strcpy(root_directory.entries[root_directory.entry_count].name, path);
    root_directory.entries[root_directory.entry_count].inode_number = inode_number;
    root_directory.entry_count++;

    FILE *file = fopen(full_path, is_directory ? "w" : "w+");
    if (file == NULL) {
        perror("Failed to create file");
        return -1;
    }
    fclose(file);

    add_journal_entry(CREATE, path, NULL, NULL);

    return 0;
}



//delete file
void delete_file(const char *filename, GtkWidget *parent) {
    char full_path[MAX_FILENAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, filename);

    if (access(full_path, 0) != 0) {
        perror("File does not exist");
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "File does not exist.");
        return;
    }


    // Remove file from disk
    if (remove(full_path) != 0) {
        perror("Failed to delete file");
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to delete file.");
        return;
    }

    // Remove file entry from the root directory and free inode
    for (int i = 0; i < root_directory.entry_count; i++) {
        if (strcmp(root_directory.entries[i].name, filename) == 0) {
            // Free blocks and reset inode
            int inode_number = root_directory.entries[i].inode_number;
            Inode *inode = inode_table[inode_number];

            for (int j = 0; j < DIRECT_BLOCKS; j++) {
                if (inode->direct_blocks[j] != -1) {
                    free_block(inode->direct_blocks[j]);
                    inode->direct_blocks[j] = -1;
                }
            }

            if (inode->index_block != -1) {
                free_index_block(inode->index_block);
                inode->index_block = -1;
            }

            inode->is_directory = 0;
            inode->_size = 0;
            inode->mtime = time(NULL);
            inode->ctime = time(NULL);

            // Remove from root directory
            memmove(&root_directory.entries[i], &root_directory.entries[i + 1], (root_directory.entry_count - i - 1) * sizeof(DirectoryEntry));
            root_directory.entry_count--;

            printf("Deleted file: %s\n", filename);
            show_message_dialog(parent, GTK_MESSAGE_INFO, "File deleted successfully.");
            break;
        }
    }

    add_journal_entry(DELETE, filename, NULL, NULL);
}


//rename file
void rename_file(const char *old_name, const char *new_name, GtkWidget *parent) {

    if (file_exists(new_name)) {
        printf("File already exists: %s\n", new_name);
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "File with that name already exists.");
        return;
    }

    char old_path[MAX_FILENAME_LEN];
    char new_path[MAX_FILENAME_LEN];
    snprintf(old_path, sizeof(old_path), "%s%s", TEST_FOLDER_PATH, old_name);
    snprintf(new_path, sizeof(new_path), "%s%s", TEST_FOLDER_PATH, new_name);

    if (rename(old_path, new_path) != 0) {
        perror("Failed to rename file");
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to rename file.");
        return;
    }

    // Update root directory
    for (int i = 0; i < root_directory.entry_count; i++) {
        if (strcmp(root_directory.entries[i].name, old_name) == 0) {
            strcpy(root_directory.entries[i].name, new_name);
            printf("Renamed file from %s to %s\n", old_name, new_name);
            show_message_dialog(parent, GTK_MESSAGE_INFO, "File renamed successfully.");
            break;
        }
    }

    add_journal_entry(RENAME, old_name, new_name, NULL);
}


//list files
void list_files(GtkWidget *widget, gpointer data) {

    GtkWidget *window = GTK_WIDGET(data);
    GtkWidget *list_view = g_object_get_data(G_OBJECT(window), "list_view");
    GtkWidget *search_entry = g_object_get_data(G_OBJECT(window), "search_entry");

    const gchar *search_text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list_view)));
    gtk_list_store_clear(list_store);

    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    char full_path[MAX_FILENAME_LEN];
    char atime_str[20], ctime_str[20], mtime_str[20];

    if ((dir = opendir(TEST_FOLDER_PATH)) == NULL) {

        perror("opendir");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {

        // Skip directories like '.' and '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // If search_text is not empty, check if the file name contains the search text

        if (strlen(search_text) > 0 && strstr(entry->d_name, search_text) == NULL) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, entry->d_name);

        if (stat(full_path, &file_stat) == -1) {

            perror("stat");
            continue;
        }

        // Format times
        strftime(atime_str, sizeof(atime_str), "%Y-%m-%d %H:%M", localtime(&file_stat.st_atime));
        strftime(ctime_str, sizeof(ctime_str), "%Y-%m-%d %H:%M", localtime(&file_stat.st_ctime));
        strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M", localtime(&file_stat.st_mtime));


        GtkTreeIter iter;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter,
                           0, entry->d_name,
                           1, S_ISDIR(file_stat.st_mode) ? "Directory" : "File",
                           2, (gint)file_stat.st_size,
                           3, atime_str,
                           4, ctime_str,
                           5, mtime_str,
                           -1);
    }

    closedir(dir);
}


//edit file
void edit_file(const char *filename, GtkWidget *parent) {

    if (strlen(filename) >= MAX_FILENAME_LEN) {
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Filename too long.");
        return;
    }

    char full_path[MAX_FILENAME_LEN + 1];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, filename);

    // Open the file
    FILE *file = fopen(full_path, "r+");
    if (file == NULL) {
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to open file for editing.");
        return;
    }

    // Read the file contents
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to determine file size.");
        return;
    }
    fseek(file, 0, SEEK_SET);

    char *file_contents = malloc(file_size + 1);
    if (file_contents == NULL) {
        fclose(file);
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to allocate memory.");
        return;
    }

    size_t bytes_read = fread(file_contents, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        free(file_contents);
        fclose(file);
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to read the entire file.");
        return;
    }
    file_contents[file_size] = '\0';  // Null-terminate the string

    fclose(file);

    // Create the editing dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Edit File",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        "_Save",
        GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 300);
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), TRUE);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, file_contents, -1);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content_area), text_view, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_ACCEPT) {
        // Save the edited content back to the file
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        const gchar *new_contents = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

        file = fopen(full_path, "w");
        if (file == NULL) {
            show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to open file for saving.");
        } else {
            fwrite(new_contents, 1, strlen(new_contents), file);
            fclose(file);
            show_message_dialog(parent, GTK_MESSAGE_INFO, "File edited successfully.");
        }
    }

    gtk_widget_destroy(dialog);
    free(file_contents);

    add_journal_entry(MODIFY, filename, NULL, NULL);
}

//open file
void open_file(const char *filename, GtkWidget *parent) {

    // Check if the file exists
    char full_path[MAX_FILENAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, filename);
    if (access(full_path, F_OK) == -1) {
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "File does not exist.");
        return;
    }

    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Yes to edit and No to view the file: %s?",
        filename
    );

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES) {
        edit_file(filename, parent);
    } else {

        // Open the file
        FILE *file = fopen(full_path, "r");
        if (file == NULL) {
            show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to open file.");
            return;
        }

        // Read the file contents
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *file_contents = malloc(file_size + 1);
        if (file_contents == NULL) {
            fclose(file);
            show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to allocate memory.");
            return;
        }

        fread(file_contents, 1, file_size, file);
        file_contents[file_size] = '\0';  // Null-terminate the string

        fclose(file);

        // Display the file contents
        GtkWidget *view_dialog = gtk_dialog_new_with_buttons(
            "File Contents",
            GTK_WINDOW(parent),
            GTK_DIALOG_MODAL,
            "_OK",
            GTK_RESPONSE_OK,
            NULL
        );

        gtk_window_set_default_size(GTK_WINDOW(view_dialog), 500, 300);
        GtkWidget *text_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        gtk_text_buffer_set_text(buffer, file_contents, -1);

        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(view_dialog));
        gtk_box_pack_start(GTK_BOX(content_area), text_view, TRUE, TRUE, 0);

        gtk_widget_show_all(view_dialog);
        gtk_dialog_run(GTK_DIALOG(view_dialog));
        gtk_widget_destroy(view_dialog);

        free(file_contents);
    }

    add_journal_entry(READ, filename, NULL, NULL);
}

void save_permissions() {

    save_file_system_state();
}

void load_permissions() {

    load_file_system_state();
}

int change_file_permissions(const char *filename, unsigned int new_mode) {

    int inode_number = find_inode_by_filename(filename);
    if (inode_number == -1) {
        return -1; // File not found
    }

    // Assuming inode_table is an array of Inode pointers
    Inode *inode = inode_table[inode_number];
    if (inode == NULL) {
        return -1; // Inode not found
    }

    inode->mode = new_mode;
    inode->ctime = time(NULL); // Update change time

    // Save permissions after changing
    save_permissions();

    // Create a journal entry for permission change
    char mode_str[12];
    snprintf(mode_str, sizeof(mode_str), "%o", new_mode);
    add_journal_entry(CHANGE_PERMISSIONS, filename, mode_str, NULL);

    return 0;
}



int create_directory(const char *path) {

    if (file_exists(path)) {
        printf("Directory already exists: %s\n", path);
        show_message_dialog(NULL, GTK_MESSAGE_INFO, "The directory already exists");
        return -1;
    }

    char full_path[MAX_FILENAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, path);

    // Create directory on disk
    if (_mkdir(full_path) != 0) {  // Default permissions for directories
        perror("Failed to create directory");
        return -1;
    }

    // Create inode for the directory
    int inode_number = create_inode(1, 0755, current_user_id, current_group_id);  // is_directory = 1 or True
    if (inode_number == -1) {
        printf("Failed to create inode for directory\n");
        return -1;
    }

    // Add directory entry to the root directory
    strcpy(root_directory.entries[root_directory.entry_count].name, path);
    root_directory.entries[root_directory.entry_count].inode_number = inode_number;
    root_directory.entry_count++;

    add_journal_entry(CREATE, path, NULL, NULL);
    show_message_dialog(NULL, GTK_MESSAGE_INFO, "Directory created successfully");

    return 0;
}

void delete_directory(const char *path, GtkWidget *parent) {

    char full_path[MAX_FILENAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, path);

    // Check if directory exists
    if (access(full_path, F_OK) != 0) {
        perror("Directory does not exist");
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Directory does not exist.");
        return;
    }

    // Remove directory from disk
    if (_rmdir(full_path) != 0) {
        perror("Failed to delete directory");
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to delete directory.");
        return;
    }

    // Remove directory entry from the root directory and free inode
    for (int i = 0; i < root_directory.entry_count; i++) {
        if (strcmp(root_directory.entries[i].name, path) == 0) {
            // Free blocks and reset inode
            int inode_number = root_directory.entries[i].inode_number;
            Inode *inode = inode_table[inode_number]; // Corrected to use pointer

            for (int j = 0; j < DIRECT_BLOCKS; j++) {
                if (inode->direct_blocks[j] != -1) {
                    free_block(inode->direct_blocks[j]);
                    inode->direct_blocks[j] = -1;
                }
            }

            if (inode->index_block != -1) {
                free_index_block(inode->index_block);
                inode->index_block = -1;
            }

            inode->is_directory = 0;
            inode->_size = 0;
            inode->mtime = time(NULL);
            inode->ctime = time(NULL);

            // Remove from root directory
            memmove(&root_directory.entries[i], &root_directory.entries[i + 1], (root_directory.entry_count - i - 1) * sizeof(DirectoryEntry));
            root_directory.entry_count--;

            printf("Deleted directory: %s\n", path);
            show_message_dialog(parent, GTK_MESSAGE_INFO, "Directory deleted successfully.");
            break;
        }
    }

    add_journal_entry(DELETE, path, NULL, NULL);
}



/* GUI FUNCTIONS */


void create_file_dialog(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog, *content_area, *entry;
    GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;
    GtkWidget *window = GTK_WIDGET(data);

    // Create and configure the dialog
    dialog = gtk_dialog_new_with_buttons("Create File",
                                         GTK_WINDOW(window),
                                         flags,
                                         "_Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "_Create",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    // Create and configure the file name entry
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter file name");
    gtk_container_add(GTK_CONTAINER(content_area), entry);

    gtk_widget_show_all(dialog);

    // Run the dialog and get user response
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_ACCEPT) {
        const gchar *filename = gtk_entry_get_text(GTK_ENTRY(entry));

        // Validate filename
        if (filename == NULL || strlen(filename) == 0) {
            show_message_dialog(window, GTK_MESSAGE_WARNING, "Filename cannot be empty");
        } else {
            // Create file with default permissions
            if (create_file(filename, 0) == 0) {
                list_files(NULL, window);
            } else {
                show_message_dialog(window, GTK_MESSAGE_ERROR, "Failed to create file");
            }
        }
    }

    gtk_widget_destroy(dialog);
}

void delete_file_dialog(GtkWidget *widget, gpointer data) {
    GtkWidget *window = GTK_WIDGET(data);
    GtkTreeView *list_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(window), "list_view"));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(list_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *filename;
        gtk_tree_model_get(model, &iter, 0, &filename, -1);

        // Confirm deletion
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_WARNING,
                                                   GTK_BUTTONS_OK_CANCEL,
                                                   "Are you sure you want to delete the file: %s?",
                                                   filename);
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        if (response == GTK_RESPONSE_OK) {
            delete_file(filename, window);
            list_files(NULL, window);
        }

        g_free(filename);
    } else {
        show_message_dialog(window, GTK_MESSAGE_ERROR, "No file selected.");
    }
}


void open_file_dialog(GtkWidget *widget, gpointer data) {

    GtkWidget *window = GTK_WIDGET(data);
    GtkTreeView *list_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(window), "list_view"));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(list_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *filename;
        gtk_tree_model_get(model, &iter, 0, &filename, -1);

        open_file(filename, window);

        g_free(filename);
    } else {
        show_message_dialog(window, GTK_MESSAGE_ERROR, "No file selected.");
    }
}

void rename_file_dialog(GtkWidget *widget, gpointer data) {

    GtkWidget *window = GTK_WIDGET(data);
    GtkTreeView *list_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(window), "list_view"));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(list_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *old_filename;
        gtk_tree_model_get(model, &iter, 0, &old_filename, -1);

        GtkWidget *dialog, *content_area, *new_entry;
        GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

        dialog = gtk_dialog_new_with_buttons("Rename File",
                                             GTK_WINDOW(window),
                                             flags,
                                             "_Cancel",
                                             GTK_RESPONSE_CANCEL,
                                             "_Rename",
                                             GTK_RESPONSE_ACCEPT,
                                             NULL);

        content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        new_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(new_entry), "Enter new file name");

        gtk_container_add(GTK_CONTAINER(content_area), new_entry);
        gtk_widget_show_all(dialog);

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));

        if (response == GTK_RESPONSE_ACCEPT) {
            const gchar *new_filename = gtk_entry_get_text(GTK_ENTRY(new_entry));
            if (new_filename != NULL && strlen(new_filename) > 0) {
                rename_file(old_filename, new_filename, window);
                list_files(NULL, window);
            }
        }

        gtk_widget_destroy(dialog);
        g_free(old_filename);
    } else {

        show_message_dialog(window, GTK_MESSAGE_ERROR, "No file selected.");
    }
}

void on_search_changed(GtkWidget *widget, gpointer data) {

    GtkWidget *window = GTK_WIDGET(data);
    list_files(NULL, window);
}

void change_permissions_response(GtkDialog *dialog, gint response_id, gpointer user_data) {

    GtkWidget *entry_filename = GTK_WIDGET(g_object_get_data(G_OBJECT(user_data), "filename_entry"));
    GtkWidget *entry_permissions = GTK_WIDGET(g_object_get_data(G_OBJECT(user_data), "permissions_entry"));

    const char *filename = gtk_entry_get_text(GTK_ENTRY(entry_filename));
    const char *permissions_str = gtk_entry_get_text(GTK_ENTRY(entry_permissions));

    if (response_id == GTK_RESPONSE_OK && strlen(filename) > 0 && strlen(permissions_str) > 0) {

        unsigned int new_mode = strtol(permissions_str, NULL, 8); // Convert permissions string to octal

        if (change_file_permissions(filename, new_mode) == 0) {
            show_message_dialog(GTK_WIDGET(dialog), GTK_MESSAGE_INFO, "Permissions changed successfully.");
        } else {
            show_message_dialog(GTK_WIDGET(dialog), GTK_MESSAGE_ERROR, "Failed to change permissions. File may not exist.");
        }
    }

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

void change_permissions_dialog(GtkWidget *widget, gpointer data) {

    GtkWidget *window = GTK_WIDGET(data);

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Change Permissions", GTK_WINDOW(window), GTK_DIALOG_MODAL, ("Change"), GTK_RESPONSE_OK, ("Cancel"), GTK_RESPONSE_CANCEL, NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *entry_filename = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_filename), "Enter file name");

    GtkWidget *entry_permissions = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_permissions), "Enter new permissions (e.g., 755)");

    gtk_box_pack_start(GTK_BOX(content_area), entry_filename, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(content_area), entry_permissions, TRUE, TRUE, 5);

    g_object_set_data(G_OBJECT(dialog), "filename_entry", entry_filename);
    g_object_set_data(G_OBJECT(dialog), "permissions_entry", entry_permissions);

    g_signal_connect(dialog, "response", G_CALLBACK(change_permissions_response), dialog);

    gtk_widget_show_all(dialog);
}

void show_file_details(const char *filename, GtkWidget *parent) {

    char full_path[MAX_FILENAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, filename);

    // Check if the file exists
    if (access(full_path, F_OK) != 0) {
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "File does not exist.");
        return;
    }

    struct stat file_stat;
    if (stat(full_path, &file_stat) == -1) {
        perror("stat");
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to get file information.");
        return;
    }

    // Format times
    char atime_str[20], ctime_str[20], mtime_str[20];
    strftime(atime_str, sizeof(atime_str), "%Y-%m-%d %H:%M", localtime(&file_stat.st_atime));
    strftime(ctime_str, sizeof(ctime_str), "%Y-%m-%d %H:%M", localtime(&file_stat.st_ctime));
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M", localtime(&file_stat.st_mtime));

    // Permissions
    char perms[11];
    snprintf(perms, sizeof(perms), "%c%c%c%c%c%c%c%c%c%c",
             S_ISDIR(file_stat.st_mode) ? 'd' : '-',
             file_stat.st_mode & S_IRUSR ? 'r' : '-',
             file_stat.st_mode & S_IWUSR ? 'w' : '-',
             file_stat.st_mode & S_IXUSR ? 'x' : '-',
             file_stat.st_mode & S_IRGRP ? 'r' : '-',
             file_stat.st_mode & S_IWGRP ? 'w' : '-',
             file_stat.st_mode & S_IXGRP ? 'x' : '-',
             file_stat.st_mode & S_IROTH ? 'r' : '-',
             file_stat.st_mode & S_IWOTH ? 'w' : '-',
             file_stat.st_mode & S_IXOTH ? 'x' : '-');

    // Create the detail dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "File Details",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL,
        "_OK",
        GTK_RESPONSE_OK,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
    gtk_container_add(GTK_CONTAINER(content_area), vbox);

    // File Details
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_text(GTK_LABEL(label),
        g_strdup_printf("Filename: %s\n"
                        "Size: %ld bytes\n"
                        "Permissions: %s\n"
                        "Access Time: %s\n"
                        "Creation Time: %s\n"
                        "Modification Time: %s",
                        filename, file_stat.st_size, perms, atime_str, ctime_str, mtime_str));
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);

    // Wait for the dialog to be closed
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void view_file_details_dialog(GtkWidget *button, gpointer user_data) {
    GtkWidget *window = GTK_WIDGET(user_data);
    GtkTreeView *list_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(window), "list_view"));

    GtkTreeSelection *selection = gtk_tree_view_get_selection(list_view);
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *filename;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter, 0, &filename, -1); // Assuming the filename is in column 0

        show_file_details(filename, window);

        g_free(filename);
    } else {
        show_message_dialog(window, GTK_MESSAGE_WARNING, "No file selected.");
    }
}


void create_directory_dialog(GtkWidget *button, gpointer user_data) {
    GtkWidget *window = GTK_WIDGET(user_data);
    GtkWidget *dialog;
    GtkWidget *entry;
    gint result;

    dialog = gtk_dialog_new_with_buttons("Create Directory",
                                         GTK_WINDOW(window),
                                         GTK_DIALOG_MODAL,
                                         "_OK",
                                         GTK_RESPONSE_OK,
                                         "_Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         NULL);

    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter new directory name");
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), entry, TRUE, TRUE, 5);
    gtk_widget_show_all(dialog);

    result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_OK) {
        const gchar *dir_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (strlen(dir_name) > 0) {
            gchar *full_path = g_strdup_printf("%s/%s", TEST_FOLDER_PATH, dir_name);
            int success = mkdir(full_path);
            if (success != 0) {
                perror("Failed to create directory");
                show_message_dialog(window, GTK_MESSAGE_ERROR, "Failed to create directory.");
            } else {
                list_files(NULL, window); // Refresh the file list
            }
            g_free(full_path);
        }
    }

    gtk_widget_destroy(dialog);
}

void ensure_trailing_slash(char *path) {
    size_t len = strlen(path);
    if (len > 0 && path[len - 1] != '/') {
        if (len < MAX_FILENAME_LEN - 1) {
            path[len] = '/';
            path[len + 1] = '\0';
        } else {
            // Handle error: path is too long
        }
    }
}
void change_directory(const char *new_path) {

    char full_path[MAX_FILENAME_LEN];
    snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, new_path);

    // Ensure the new path ends with a '/'
    ensure_trailing_slash(full_path);

    // Check if the new path exists and is a directory
    struct stat statbuf;
    if (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        // Save the current root directory as the previous root
        snprintf(PREVIOUS_ROOT_PATH, sizeof(PREVIOUS_ROOT_PATH), "%s", TEST_FOLDER_PATH);

        // Update the global path to the new root directory
        snprintf(TEST_FOLDER_PATH, sizeof(TEST_FOLDER_PATH), "%s", full_path);

        if (_chdir(TEST_FOLDER_PATH) == 0) {
            printf("Changed directory to new root: %s\n", TEST_FOLDER_PATH);
            // Optionally refresh file list or other UI updates here
        } else {
            perror("Failed to change directory");
        }
    } else {
        printf("Invalid directory path: %s\n", full_path);
        // Optionally show an error dialog
    }
}

void change_directory_dialog(GtkWidget *widget, gpointer data) {

    GtkWidget *window = GTK_WIDGET(data);
    GtkTreeView *list_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(window), "list_view"));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(list_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *dirname;
        gtk_tree_model_get(model, &iter, 0, &dirname, -1);

        char full_path[MAX_FILENAME_LEN];
        snprintf(full_path, sizeof(full_path), "%s%s", TEST_FOLDER_PATH, dirname);

        // Ensure trailing slash for directories
        size_t len = strlen(full_path);
        if (dirname[strlen(dirname) - 1] != '/') {
            full_path[len] = '/';
            full_path[len + 1] = '\0';
        }

        // Check if the new path exists and is a directory
        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            if (_chdir(full_path) == 0) {

                printf("Changed directory to: %s\n", full_path);

                // Update TEST_FOLDER_PATH
                snprintf(TEST_FOLDER_PATH, sizeof(TEST_FOLDER_PATH), "%s", full_path);

                // Update the file list
                list_files(NULL, window);
            } else {
                perror("Failed to change directory");
            }
        } else {
            printf("Invalid directory path: %s\n", full_path);
            show_message_dialog(window, GTK_MESSAGE_ERROR, "Invalid directory path");
        }

        g_free(dirname);
    } else {
        show_message_dialog(window, GTK_MESSAGE_ERROR, "No directory selected.");
    }
}




void go_back_to_previous_root(GtkWidget *window) {

    // Check if we are not already in the previous root
    if (strcmp(TEST_FOLDER_PATH, PREVIOUS_ROOT_PATH) != 0) {
        // Attempt to change to the previous root directory
        if (_chdir(PREVIOUS_ROOT_PATH) == 0) {
            // Update the current root directory path
            snprintf(TEST_FOLDER_PATH, sizeof(TEST_FOLDER_PATH), "%s", PREVIOUS_ROOT_PATH);
            printf("Returned to previous root directory: %s\n", TEST_FOLDER_PATH);

            // Refresh the file list to show contents of the new directory
            list_files(NULL, window);
        } else {
            perror("Failed to change directory");
        }
    } else {
        printf("You are already in the previous root directory.\n");
        // Optionally show a message to the user
        show_message_dialog(window, GTK_MESSAGE_INFO, "You are already in the previous root directory.");
    }
}


void go_back_button_clicked(GtkWidget *widget, gpointer window) {

    go_back_to_previous_root(window);
}


void delete_directory_dialog(GtkWidget *widget, gpointer window) {
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *entry;

    dialog = gtk_dialog_new_with_buttons("Delete Directory",
                                         GTK_WINDOW(window),
                                         GTK_DIALOG_MODAL,
                                         ("_OK"),
                                         GTK_RESPONSE_OK,
                                         ("_Cancel"),
                                         GTK_RESPONSE_CANCEL,
                                         NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter directory name");
    gtk_box_pack_start(GTK_BOX(content_area), entry, FALSE, FALSE, 5);
    gtk_widget_show_all(dialog);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK) {
        const char *dir_name = gtk_entry_get_text(GTK_ENTRY(entry));
        if (strlen(dir_name) > 0) {
            delete_directory(dir_name, window);
        } else {
            show_message_dialog(window, GTK_MESSAGE_ERROR, "Directory name cannot be empty");
        }
    }
    gtk_widget_destroy(dialog);
}

char copied_file_path[256] = ""; // Buffer to store the path of the copied file

void copy_file(const char *filename, GtkWidget *parent) {
    snprintf(copied_file_path, sizeof(copied_file_path), "%s%s", TEST_FOLDER_PATH, filename);
    show_message_dialog(parent, GTK_MESSAGE_INFO, "File copied to clipboard.");
}
void paste_file(GtkWidget *parent) {
    if (strcmp(copied_file_path, "") == 0) {
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "No file copied.");
        return;
    }

    char dest_path[256];
    snprintf(dest_path, sizeof(dest_path), "%s%s_copy", TEST_FOLDER_PATH, strrchr(copied_file_path, '/') + 1);

    FILE *src = fopen(copied_file_path, "rb");
    if (src == NULL) {
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to open source file.");
        return;
    }

    FILE *dest = fopen(dest_path, "wb");
    if (dest == NULL) {
        fclose(src);
        show_message_dialog(parent, GTK_MESSAGE_ERROR, "Failed to create destination file.");
        return;
    }

    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }

    fclose(src);
    fclose(dest);

    show_message_dialog(parent, GTK_MESSAGE_INFO, "File pasted successfully.");
    list_files(NULL, parent); // Refresh file list
}

void on_copy_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *window = GTK_WIDGET(data);
    GtkTreeView *list_view = GTK_TREE_VIEW(g_object_get_data(G_OBJECT(window), "list_view"));
    GtkTreeSelection *selection = gtk_tree_view_get_selection(list_view);
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *filename;
        gtk_tree_model_get(model, &iter, 0, &filename, -1);
        copy_file(filename, window);
        g_free(filename);
    } else {
        show_message_dialog(window, GTK_MESSAGE_ERROR, "No file selected.");
    }
}

void on_paste_button_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *window = GTK_WIDGET(data);
    paste_file(window);
}


//User Interface Initialization

void initialize_gui(GtkWidget **window, GtkWidget **list_view, GtkWidget **search_entry, GtkWidget **vbox) {

    GtkWidget *toolbar, *scrolled_window, *folder_label;
    GtkToolItem *create_button, *delete_button, *rename_button, *refresh_button,
                *permissions_button, *details_button, *create_dir_button,
                *change_dir_button, *go_back_button,
                *delete_dir_button,* copy_button, * paste_button;

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkListStore *list_store;
    GtkToolItem *open_button;
    GFileMonitor *monitor;

    *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(*window), "File System Emulation");
    gtk_window_set_default_size(GTK_WINDOW(*window), 800, 800);

    g_signal_connect(*window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(*window), *vbox);

    toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    gtk_box_pack_start(GTK_BOX(*vbox), toolbar, FALSE, FALSE, 5);

    go_back_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_GO_BACK, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), go_back_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(go_back_button), "Go back to previous directory");
    g_signal_connect(go_back_button, "clicked", G_CALLBACK(go_back_button_clicked), *window);

    copy_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), copy_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(copy_button), "Copy file");
    g_signal_connect(copy_button, "clicked", G_CALLBACK(on_copy_button_clicked), *window);

    paste_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_PASTE, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), paste_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(paste_button), "Paste file");
    g_signal_connect(paste_button, "clicked", G_CALLBACK(on_paste_button_clicked), *window);

    open_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), open_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(open_button), "Open a file");
    g_signal_connect(open_button, "clicked", G_CALLBACK(open_file_dialog), *window);

    create_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), create_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(create_button), "Create file");
    g_signal_connect(create_button, "clicked", G_CALLBACK(create_file_dialog), *window);


    create_dir_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_DIRECTORY, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), create_dir_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(create_dir_button), "Create a new directory");
    g_signal_connect(create_dir_button, "clicked", G_CALLBACK(create_directory_dialog), *window);


    delete_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_DELETE, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), delete_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(delete_button), "Delete a file");
    g_signal_connect(delete_button, "clicked", G_CALLBACK(delete_file_dialog), *window);

    rename_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), rename_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(rename_button), "Rename a file");
    g_signal_connect(rename_button, "clicked", G_CALLBACK(rename_file_dialog), *window);

    details_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_PROPERTIES, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), details_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(details_button), "View Details");
    g_signal_connect(details_button, "clicked", G_CALLBACK(view_file_details_dialog), *window);

    permissions_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_PREFERENCES , GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), permissions_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(permissions_button), "Change Permissions");
    g_signal_connect(permissions_button, "clicked", G_CALLBACK(change_permissions_dialog), *window);

    refresh_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_REFRESH, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), refresh_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(refresh_button), "Refresh List");
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(list_files), *window);

    delete_dir_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_REMOVE, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), delete_dir_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(delete_dir_button), "Delete a directory");
    g_signal_connect(delete_dir_button, "clicked", G_CALLBACK(delete_directory_dialog), *window);

    change_dir_button = gtk_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_LARGE_TOOLBAR), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), change_dir_button, -1);
    gtk_widget_set_tooltip_text(GTK_WIDGET(change_dir_button), "Change directory");
    g_signal_connect(change_dir_button, "clicked", G_CALLBACK(change_directory_dialog), *window);

    GtkWidget *description_label = gtk_label_new("Root Directory:");
    folder_label = gtk_label_new(TEST_FOLDER_PATH);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(hbox), description_label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), folder_label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(*vbox), hbox, FALSE, FALSE, 5);

    *search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(*search_entry), "Search ....");
    GtkWidget *search_icon = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), *search_entry, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), search_icon, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(*vbox), hbox, FALSE, FALSE, 5);
    g_signal_connect(*search_entry, "changed", G_CALLBACK(on_search_changed), *window);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(*vbox), scrolled_window, TRUE, TRUE, 5);

    list_store = gtk_list_store_new(6, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    *list_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_container_add(GTK_CONTAINER(scrolled_window), *list_view);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(*list_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Type", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(*list_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Size (bytes)", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(*list_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Last Accessed", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(*list_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Date Created", renderer, "text", 4, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(*list_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Date Modified", renderer, "text", 5, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(*list_view), column);

    g_object_set_data(G_OBJECT(*window), "list_view", *list_view);
    g_object_set_data(G_OBJECT(*window), "search_entry", *search_entry);

    gtk_widget_show_all(*window);
}


int main(int argc, char *argv[]) {


    GtkWidget *window, *list_view, *search_entry;
    GtkWidget *vbox;

    gtk_init(&argc, &argv);

    initialize_gui(&window, &list_view, &search_entry, &vbox);

    // Initialize the file system and populate the file list
    init_file_system();
    list_files(NULL, window);

    // Monitor the directory for changes
    GFile *directory = g_file_new_for_path(TEST_FOLDER_PATH);
    GFileMonitor *monitor = g_file_monitor_directory(directory, G_FILE_MONITOR_NONE, NULL, NULL);
    g_signal_connect(monitor, "changed", G_CALLBACK(on_directory_changed), window);

    gtk_widget_show_all(window);
    gtk_main();

    // Free allocated memory
    free_memory();

    return 0;
}
