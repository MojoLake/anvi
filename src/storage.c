#define _GNU_SOURCE

#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/timerfd.h>

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/config.h>


static int
create_fd_with_unoccupied_file_name(char *dir_path, char* created_path, size_t created_path_capacity) {
    time_t now = time(NULL);
    struct tm local_time;

    if (now == (time_t)-1 || localtime_r(&now, &local_time) == NULL) {
        return -1;
    }

    char stem[64];

    if (strftime( stem, sizeof stem, "anvi-%Y-%m-%d-%H%M", &local_time) == 0) {
        errno = EOVERFLOW;
        return -1;
    }

    for (size_t i = 0; i < 10000; ++i) {
        char name[256];

        int name_length;

        if (i == 0) {
            name_length = snprintf(name, sizeof name, "%s.txt", stem);
        } else {
            name_length = snprintf(name, sizeof name, "%s-%zu.txt", stem, i);
        }

        if (name_length < 0 || (size_t)name_length >= sizeof name) {
            errno = ENAMETOOLONG;
            return -1;
        }

        int path_length = snprintf(created_path, created_path_capacity, "%s/%s", dir_path, name);

        if (path_length < 0 || (size_t)path_length >= created_path_capacity) {
            errno = ENAMETOOLONG;
            return -1;
        }

        int fd = open(created_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);

        if (fd >= 0) {
            return fd;
        }

        if (errno != EEXIST) {
            return -1;
        }
    }

    errno = EEXIST;
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

static int
set_default_save_path(char *directory) {
    const char *home = getenv("HOME"); 
    if (home == NULL || home[0] == '\0') {
        anvi_log_error("Couldn't figure out what the user's home directory is.");
        return EXIT_FAILURE;
    }

    
    const int n = snprintf(directory, MAX_FILE_PATH_LEN, "%s/anvi", home);
    if (n < 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int
create_document_fd(struct anvi_storage *storage, char *save_path) {
    char directory[4096];
    if (save_path == NULL || save_path[0] == '\0') {
        if (set_default_save_path(directory) == EXIT_FAILURE) {
            return EXIT_FAILURE;
        }
    } else {
        strcpy(directory, save_path);
    }

    if (create_directory(directory) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    int fd = create_fd_with_unoccupied_file_name(directory, storage->path_for_document_fd, sizeof storage->path_for_document_fd);

    if (fd == -1) {
        anvi_log_error("Failed to create file descript for persisting the document: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    storage->document_fd = fd;

    return EXIT_SUCCESS;
}

int
write_bytes_to_document_fd(struct anvi_storage *storage, struct anvi_text_buffer *doc) {
    anvi_log_info("Saving document to disk.");

    if (ftruncate(storage->document_fd, 0) == -1) {
        return EXIT_FAILURE;
    }

    if (lseek(storage->document_fd, 0, SEEK_SET) == -1) {
        return EXIT_FAILURE;
    }

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

int
create_save_timer_fd(struct anvi_storage *storage) {
    storage->save_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);

    if (storage->save_timer_fd < 0) {
        anvi_log_error("Failed to create storage save timer.");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int
start_save_timer(struct anvi_storage *storage) {
    struct itimerspec timer = {0};

    timer.it_value.tv_sec = SAVE_EVERY_MS / 1000;
    timer.it_value.tv_nsec = (SAVE_EVERY_MS % 1000) * 1000000L;

    if (timerfd_settime(storage->save_timer_fd, 0, &timer, NULL) < 0) {
        anvi_log_error("Failed to start save timer.");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int
handle_save_timer(struct anvi_storage *storage, struct anvi_text_buffer *doc) {

    uint64_t expirations;
    ssize_t bytes_read = read(storage->save_timer_fd, &expirations, sizeof(expirations));

    if (bytes_read == sizeof(expirations)) {
        write_bytes_to_document_fd(storage, doc); 
    }
    
    // Start a new timer.
    return start_save_timer(storage);
}
