#include <stdbool.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <anvi/log.h>
#include <anvi/save_dialog.h>

struct save_dialog_context {
    GMainLoop *loop;
    const char *recovery_path;
    int result;
};

static void
save_dialog_finished(GObject *source_object, GAsyncResult *async_result, gpointer user_data) {
    struct save_dialog_context *context = user_data;
    GError *error = NULL;

    GFile *destination = gtk_file_dialog_save_finish(
            GTK_FILE_DIALOG(source_object),
            async_result,
            &error
    );

    if (destination == NULL) {
        if (error != NULL && g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED)) {
            anvi_log_info("Save dialog cancellled. The document remains at %s", context->recovery_path);

            // Cancellation is not data loss so not treated as an application error.
            context->result = EXIT_SUCCESS;
        } else {
        anvi_log_error("The save dialog failed: %s. The document remains at %s", error != NULL ? error->message : "unknown error", context->recovery_path);
        }

        g_clear_error(&error);
        g_main_loop_quit(context->loop);
        return;
    } 

    GFile *source = g_file_new_for_path(context->recovery_path);

    if (g_file_equal(source, destination)) {
        context->result = EXIT_SUCCESS;
    } else if (g_file_copy(source, destination, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
        context->result = EXIT_SUCCESS;
    } else {
        anvi_log_error("Failed to save the selected file: %s. The document remains at %s", error->message, context->recovery_path);
    }

    if (context->result == EXIT_SUCCESS) {
        char *destination_name = g_file_get_parse_name(destination);

        anvi_log_info("Saved a copy of the document to %s. The recovery file remains at %s", destination_name, context->recovery_path);

        g_free(destination_name);
    }

    g_clear_error(&error);
    g_object_unref(source);
    g_object_unref(destination);
    g_main_loop_quit(context->loop);
}

int
anvi_show_save_dialog(const char *recovery_path) {
    if (recovery_path == NULL || recovery_path[0] == '\0') {
        anvi_log_error("Cannot open the save dialog without a recovery path.");
        return EXIT_FAILURE;
    }

    if (!gtk_init_check()) {
        anvi_log_error("GTK could not open a display. The document remains at %s", recovery_path);
        return EXIT_FAILURE;
    }

    GMainLoop *loop = g_main_loop_new(NULL, false);
    GtkFileDialog *dialog = gtk_file_dialog_new();
    char *suggested_name = g_path_get_basename(recovery_path);

    gtk_file_dialog_set_title(dialog, "Save Anvi document");
    gtk_file_dialog_set_accept_label(dialog, "Save");
    gtk_file_dialog_set_initial_name(dialog, suggested_name);

    struct save_dialog_context context = {
        .loop = loop,
        .recovery_path = recovery_path,
        .result = EXIT_FAILURE,
    };

    gtk_file_dialog_save(dialog, NULL, NULL, save_dialog_finished, &context);

    g_main_loop_run(loop);

    g_free(suggested_name);
    g_object_unref(dialog);
    g_main_loop_unref(loop);

    return context.result;
}
