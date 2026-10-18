#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/output.h>
#include <anvi/buffer.h>
#include <anvi/keyboard.h>
#include <anvi/session_setup.h>
#include <anvi/config.h>
#include <anvi/save_dialog.h>


int main(void) {
    struct anvi_state state = {0};
    struct anvi_config config = {0};

    bool show_save_dialog = false;
    char recovery_path[ANVI_STORAGE_PATH_CAPACITY] = {0};

    if (anvi_config_set_defaults(&config) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }
    if (anvi_config_load(&config) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    if (setup_initial_state(&state, &config) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    anvi_log_info("Initial state setup successfully.");

    while (true) {

        wl_display_dispatch_pending(state.display);
        wl_display_prepare_read(state.display);
        wl_display_flush(state.display);

        if (poll_for_events_and_timer_completion(&state) == EXIT_FAILURE) {
            return EXIT_FAILURE;
        };

        if (state.session_is_finished) {
            safe_unlock_and_destroy_session_lock(&state);
            break;
        }

        int should_break = 0;
        switch (state.phase) {
            case ANVI_START_CONFIGURATION_PHASE:
                should_break = handle_start_configuration_phase_exit_check(&state);
                break;
            case ANVI_NORMAL_PHASE:
                should_break = handle_normal_phase_exit_check(&state);
                break;
            case ANVI_FINISHED_PHASE:
                should_break = handle_finish_phase_exit_check(&state);
                break;
        }

        if (should_break) {
            bool completed_writing = state.phase == ANVI_FINISHED_PHASE;

            wl_display_roundtrip(state.display);
            if (write_bytes_to_document_fd(state.storage, state.document) == EXIT_FAILURE) {
                anvi_log_error("Failed to write the final document to the recovery file.");
                destroy_anvi_state(&state);
                return EXIT_FAILURE;
            }

            if (completed_writing) {
                int path_length = snprintf(recovery_path, sizeof recovery_path, "%s", state.storage->path_for_document_fd);

                if (path_length < 0 || (size_t)path_length >= sizeof recovery_path) {
                    anvi_log_error("The recovery path is too long to pass to the dialog.");

                    destroy_anvi_state(&state);
                    return EXIT_FAILURE;
                }

                show_save_dialog = true;
            }
            break;
        }
    }

    destroy_anvi_state(&state);

    if (show_save_dialog) {
        return anvi_show_save_dialog(recovery_path, config.save_dir);
    }
	return EXIT_SUCCESS;
}
