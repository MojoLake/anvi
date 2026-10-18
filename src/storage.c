#define _GNU_SOURCE

#include <glib.h>
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
create_fd_with_unoccupied_file_name(char *recovery_directory, const char* final_save_directory, char* created_path, size_t created_path_capacity) {
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

        char final_path[ANVI_STORAGE_PATH_CAPACITY];
        int final_path_length = snprintf(final_path, sizeof final_path, "%s/%s", final_save_directory, name);

        if (final_path_length < 0 || (size_t)final_path_length >= sizeof final_path) {
            errno = ENAMETOOLONG;
            return -1;
        }

        if (access(final_path, F_OK) == 0) {
            continue;
        }

        if (errno != ENOENT) {
            return -1;
        }

        int recovery_path_length = snprintf(created_path, created_path_capacity, "%s/%s", recovery_directory, name);

        if (recovery_path_length < 0 || (size_t)recovery_path_length >= created_path_capacity) {
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
create_directory(const char *path) {
    if (g_mkdir_with_parents(path, 0700) == -1) {
        anvi_log_error("Could not create directory %s: %s", path, strerror(errno));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int
create_document_fd(struct anvi_storage *storage, const char *save_path) {
    if (save_path == NULL || save_path[0] == '\0') {
        anvi_log_error("The final save directory is empty.");
        return EXIT_FAILURE;
    } 

    if (create_directory(save_path) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    char *recovery_directory = g_build_filename(g_get_user_state_dir(), "anvi", "recovery", NULL);

    if (create_directory(recovery_directory) == EXIT_FAILURE) {
        g_free(recovery_directory);
        return EXIT_FAILURE;
    }

    int fd = create_fd_with_unoccupied_file_name(recovery_directory, save_path, storage->path_for_document_fd, sizeof storage->path_for_document_fd);

    if (fd == -1) {
        int saved_errno = errno;

        g_free(recovery_directory);

        anvi_log_error("Failed to create the recovery file: %s", strerror(saved_errno));
        return EXIT_FAILURE;
    }

    storage->document_fd = fd;

    anvi_log_info("Recovery document created at %s", storage->path_for_document_fd);

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
