#define _GNU_SOURCE

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


int main(void) {
    struct anvi_state state = {0};

    if (setup_initial_state(&state) == EXIT_FAILURE) {
        return EXIT_FAILURE;
    }

    anvi_log_info("Initial state setup successfully.\n");

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
            wl_display_roundtrip(state.display);
            write_bytes_to_document_fd(state.storage, state.document);
            break;
        }
    }

    destroy_anvi_state(&state);
	return EXIT_SUCCESS;
}
