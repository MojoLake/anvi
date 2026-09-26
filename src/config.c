#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <anvi/app.h>
#include <anvi/log.h>
#include <anvi/config.h>

void
anvi_config_set_defaults(struct anvi_config *config) {
    char path[] = "/home/mojolake/.config/anvi";
    for (size_t i = 0; i < sizeof(path); ++i) {
        config->save_dir[i] = path[i];
    }
}

enum anvi_config_key {
    ANVI_CONFIG_KEY_SAVE_DIRECTORY,
};

struct anvi_config_line_result {
    bool ok;
    enum anvi_config_key key;
    char value[MAX_FILE_PATH_LEN];
};

static char *
trim_whitespace(char *text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }

    size_t length = strlen(text);

    while (length > 0 && isspace((unsigned char)text[length -1])) {
        text[length - 1] = '\0';
        length -= 1;
    }

    return text;
}

static void
get_config_line_result(char *line, struct anvi_config_line_result *res) { // [x, y) interval
    res->ok = false;

    char *text = trim_whitespace(line);
    if (*text == '\0' || *text == '#') {
        res->ok = true;
        return;
    }

    char *equals = strchr(text, '=');
    if (equals == NULL) {
        return;
    }

    *equals = '\0';
    char *key = trim_whitespace(text);
    char *value = trim_whitespace(equals + 1);

    if (strcmp(key, "save_directory") != 0) {
        return;
    }

    size_t value_len = strlen(value);

    // Remove potential quotes
    if (value_len >= 2 && value[0] == '"' && value[value_len - 1] == '"') {
        value[value_len - 1] = '\0';
        value++;
        value_len -= 2;
    } else if (value_len > 0 && (value[0] == '"' || value[value_len - 1] == '"')) {
        return; // Unmatched quote
    }

    if (value_len == 0 || value_len >= sizeof(res->value)) {
        return;
    }

    // For now the only key.
    res->key = ANVI_CONFIG_KEY_SAVE_DIRECTORY;
    memcpy(res->value, value, value_len + 1);
    res->ok = true;
}

static void
handle_save_directory_config_key(const char *dir, struct anvi_config *config) {
    memcpy(config->save_dir, dir, strlen(dir) + 1);
}

static void
handle_config_line_result(struct anvi_config_line_result *res, struct anvi_config *config) {

    switch (res->key) {
        case ANVI_CONFIG_KEY_SAVE_DIRECTORY:
            handle_save_directory_config_key(res->value, config);
            break;
    }
}

int
anvi_config_load(struct anvi_config *config) {
    // First read the user configuration.
    // $XDG_CONFIG_HOME, $HOME, built-in default?
    FILE *file = fopen("/home/mojolake/code/projects/anvi/anvi.conf", "r");
    if (file == NULL) {
        return exit_with_failure_and_message("Failed to open configuration file.");
    }

    char buffer[1024];
    size_t line = 0;
    while (fgets(buffer, sizeof buffer, file) != NULL) {
        line++;

        if (strchr(buffer, '\n') == NULL && !feof(file)) {
            fclose(file);
            return exit_with_failure_and_message("configuration line is too long.");
        }

        struct anvi_config_line_result config_line_result;
        get_config_line_result(buffer, &config_line_result);

        if (!config_line_result.ok) {
            fclose(file);
            anvi_log_error("Invalid configuration at line %zu", line);
            return EXIT_FAILURE;
        }

        if (trim_whitespace(buffer)[0] != '\0' && trim_whitespace(buffer)[0] != '#') {
            handle_config_line_result(&config_line_result, config);
        }
    }

    if (ferror(file)) {
        fclose(file);
        return exit_with_failure_and_message("fgets failed");
    }
    fclose(file);

    return EXIT_SUCCESS;
}
