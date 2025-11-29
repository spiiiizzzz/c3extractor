#include <asm-generic/errno-base.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

typedef struct {
    char magic1[4];          
    char unknown[12];
    char magic2[4];
    uint32_t unknown2;
    uint32_t unknown3;
    uint32_t entry_count;
} header;

typedef struct __attribute__((__packed__)) {
    uint64_t unknown;               // Didn't find a use for these first bytes. They don't even seem necessary for the extraction
    uint64_t offset;                // The offset from the start of the blob. Not used in this current implementation but still included.
    uint64_t file_size;             // Second and third both store the size of the entry
    uint64_t file_size_duplicate;   // Why are they duplicated? I dunno i didn't make this format
    uint32_t unknown2;
    char char_count;
    char* name;
} entry;

// Full disclosure: this function was vibe-coded. It's the only part of the program that was.
void create_directories(const char *path) {
    char dirpath[512];
    strncpy(dirpath, path, sizeof(dirpath));
    
    // Remove the last component (the file name)
    char *last_slash = strrchr(dirpath, '/');
    if (last_slash != NULL) {
        *last_slash = '\0'; // Null-terminate the directory string

        // Create directories recursively
        for (char *p = dirpath + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0'; // Temporarily terminate the string to create the directory
                if (mkdir(dirpath, 0777) == -1 && errno != EEXIST) {
                    perror("Error creating directory");
                }
                *p = '/'; // Restore the string
            }
        }
        
        // Create the final directory if it doesn't exist
        if (mkdir(dirpath, 0777) == -1 && errno != EEXIST) {
            perror("Error creating directory");
        }
    }
}

int main(int argc, char** argv) {
    char* pname = argv[0];
    argc--;
    argv = &argv[1];

    char* destination = NULL;
    char* source = NULL;

    while (argc > 0) {
        if (strcmp(argv[0], "-o") == 0 && argc >= 2) {
            destination = argv[1];
            argc-=2;
            argv = &argv[2];
        } else {
            source = argv[0];
            argc--;
            argv = &argv[1];
        }
    }

    if (!source) {
        fprintf(stderr, "No input given. Usage: %s [input file.dat] <-o [destination folder/directory](optional)>\n", pname);
        exit(1);
    }

    int f = open(source, O_RDONLY);

    if (f == -1) {
        fprintf(stderr, "Error: Could not open file: %s\n", strerror(errno));
        exit(1);
    }

    // You can expect two leading headers at the start of the bundle
    header h = {0};
    
    read(f, &h, sizeof(header));

    // check if the file is actually a construct 3 archive
    if (strcmp(h.magic1, "c3ab") != 0 || strcmp(h.magic2, "fdir") != 0) {
        fprintf(stderr, "Error: File is not a construct 3 archive\n");
        exit(1);
    }
    
    size_t count = __builtin_bswap32(h.entry_count);

    entry *entries = malloc(sizeof(entry)*count); 

    // This is the part where the filenames are read
    for (size_t i = 0; i < count; i++) {
        entry *e = &entries[i];

        read(f, e, sizeof(entry) - sizeof(char*));

        e->offset = __builtin_bswap64(e->offset);
        e->file_size = __builtin_bswap64(e->file_size);
        e->file_size_duplicate = __builtin_bswap64(e->file_size_duplicate);

        e->name = malloc(e->char_count+1);
        read(f, e->name, e->char_count);
        e->name[e->char_count] = '\0';
    }

    // What's this? The makers of the protocol felt it was necessary to mark the start of the actual files
    // It's useless really since you already know the amount of entries and can infer their size, and therefore you
    // can infer where the actual files start, but whatever
    char blob_delimiter[12];
    read(f, &blob_delimiter, 12);

    // This is the part where we actually extract the files

    if (destination) {
        int res;
        res = mkdir(destination, S_IRWXU);
        if (res != 0 && errno != EEXIST) {
            fprintf(stderr, "Error: could not create destination directory: %s\n", strerror(errno));
            exit(1);
        } 
        res = chdir(destination);
        if (res != 0) {
            fprintf(stderr, "Error: could not change directory to destination directory: %s\n", strerror(errno));
            exit(1);
        } 
    }

    for (size_t i = 0; i < count; i++) {
        size_t file_size = entries[i].file_size;

        char* file_buff = malloc(file_size);
    
        read(f, file_buff, file_size);

        create_directories(entries[i].name);

        int new_file = open(entries[i].name, O_CREAT | O_WRONLY, S_IRWXU);
        if (new_file == -1) {
            fprintf(stderr, "Error: Could not open file: %s\n", strerror(errno));
            exit(1);
        }
        write(new_file, file_buff, file_size);

        close(new_file);

        free(file_buff);
    }

    close(f);

    // These printf's where used for debug, I'm leaving only the last one since it's the only one that's somewhat useful
    //printf("blob delimiter: %s - %ld\n", blob_delimiter, __builtin_bswap64((uint64_t)&(blob_delimiter[4])));

    //printf("s1: %s\n", s1.magic);
    //printf("s2: %s - %x\n", s2.magic, s2.entry_count);

    for (size_t i = 0; i < count; i++) {
        entry *e = &entries[i];

        printf("Entry: %s - %d - %ld - %ld - %ld\n", e->name, e->char_count, e->file_size, e->file_size_duplicate, e->offset);
    }

    for (size_t i = 0; i < count; i++) {
        entry *e = &entries[i];
        free(e->name);
    }
    free(entries);

    return 0;
}