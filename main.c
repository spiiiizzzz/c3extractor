#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

typedef struct {
    char magic[4];          // Where the magic numbers are located, didn't find any purpose beyond that
    char unknown[4];        // Could not find any use for these
    uint32_t unknown2;       
    uint32_t entry_count;   // This only applies to the second header;
} header;                   // The name doesn't really explain much, it's used for the first leading bytes of the assets bundle

typedef struct {
    char unknown[20];               // Didn't find a use for these first bytes. They don't even seem necessary for the extraction
    uint64_t file_size;             // Second and third both store the size of the entry
    uint64_t file_size_duplicate;   // Why are they duplicated? I dunno i didn't make this format
    char char_count;
    char* name;
} entry;

uint32_t swap_endianness(uint32_t x) {
    return ((x >> 24) & 0x000000FF) | 
           ((x >> 8) & 0x0000FF00) | 
           ((x << 8) & 0x00FF0000) | 
           ((x << 24) & 0xFF000000);
}

uint64_t swap_endianness_64(uint64_t x) {
    return ((x >> 56) & 0x00000000000000FFULL) | 
           ((x >> 40) & 0x000000000000FF00ULL) | 
           ((x >> 24) & 0x00000000FF000000ULL) | 
           ((x >> 8)  & 0x00FF000000000000ULL) | 
           ((x << 8)  & 0xFF00000000000000ULL) | 
           ((x << 24) & 0x000000FF00000000ULL) | 
           ((x << 40) & 0x0000FF0000000000ULL) | 
           ((x << 56) & 0x00FF000000000000ULL);
}

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
    header s1 = {0};
    header s2 = {0};
    
    read(f, &s1, sizeof(header));
    read(f, &s2, sizeof(header));

    // check if the file is actually a construct 3 archive
    if (strcmp(s1.magic, "c3ab") != 0 || strcmp(s2.magic, "fdir") != 0) {
        fprintf(stderr, "Error: File is not a construct 3 archive\n");
        exit(1);
    }
    
    s2.entry_count = swap_endianness(s2.entry_count);
    
    size_t count = s2.entry_count;

    entry *entries = malloc(sizeof(entry)*count); 

    // This is the part where the filenames are read
    for (size_t i = 0; i < count; i++) {
        entry *e = &entries[i];

        read(f, &e->unknown, 20);
        read(f, &e->file_size, 8);
        read(f, &e->file_size_duplicate, 8);
        e->file_size = swap_endianness(e->file_size);
        e->file_size_duplicate = swap_endianness(e->file_size_duplicate);
        read(f, &e->char_count, 1);
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
        if (res != 0) {
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
    //printf("blob delimiter: %s - %ld\n", blob_delimiter, swap_endianness_64((uint64_t)&(blob_delimiter[4])));

    //printf("s1: %s\n", s1.magic);
    //printf("s2: %s - %x\n", s2.magic, s2.entry_count);

    for (size_t i = 0; i < count; i++) {
        entry *e = &entries[i];

        printf("Entry: %s - %d - %ld - %ld\n", e->name, e->char_count, e->file_size, e->file_size_duplicate);
    }

    for (size_t i = 0; i < count; i++) {
        entry *e = &entries[i];
        free(e->name);
    }
    free(entries);

    return 0;
}