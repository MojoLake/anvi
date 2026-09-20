#define _GNU_SOURCE

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include <anvi/app.h>
#include <anvi/log.h>


static int
create_fd_with_unoccupied_file_name(char *dir_path, char* start_name) {
    
    char name[256];
    char full_path[4096];

    for (size_t i = 0; i < 10000; ++i) {
        const int n = snprintf(name, sizeof name, "%s_%zu.txt", start_name, i);

        if (n < 0) continue;

        const int m = snprintf(full_path, sizeof full_path, "%s/%s", dir_path, name);

        if (m < 0) continue;

        int fd = open(full_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);

        if (fd < 0) continue;
        return fd;
    }
    return -1; 
}

static int
create_directory(char *path) {
    if (mkdir(path, 0700) == -1) {
        if (errno != EEXIST) {
            anvi_log_error("Wasn't able to create the directory for the text file.");
            return EXIT_FAILURE;
        }

        struct stat st;
        if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
            anvi_log_error("Something that is not a directory already exists in this path.");
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int
create_document_fd(struct anvi_storage *storage) {
    const char *home = getenv("HOME"); 
    if (home == NULL || home[0] == '\0') {
        anvi_log_error("Couldn't figure out what the user's home directory is.");
        return EXIT_FAILURE;
    }

    char directory[4096];
    
    const int n = snprintf(directory, sizeof directory, "%s/anvi", home);
    if (n < 0) {
        return EXIT_FAILURE;
    }

    if (create_directory(directory) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    int fd = create_fd_with_unoccupied_file_name(directory, "Unknown");

    if (fd == -1) {
        anvi_log_error("Failed to create file descript for persisting the document: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    storage->document_fd = fd;

    return EXIT_SUCCESS;
}

int
write_bytes_to_document_fd(struct anvi_storage *storage, struct anvi_text_buffer *doc) {
    size_t offset = 0;
    while (offset < doc->length_bytes) {
        ssize_t n = write(storage->document_fd, doc->data + offset, doc->length_bytes - offset);

        if (n > 0) {
            offset += (size_t)n;
        } else if (n == -1 && errno == EINTR){
            continue;
        } else {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
