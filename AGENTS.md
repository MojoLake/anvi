# Repository Instructions

- Do not create, modify, delete, rename, or otherwise write to project files unless the user explicitly asks for those file changes.
- Requests for explanations, guidance, reviews, suggestions, or examples are not authorization to edit files.
- Before making any filesystem change, verify that the user's current request explicitly authorizes it.
- Read-only inspection is allowed when it helps answer the user's question.

# Anvi

Anvi is a C23 Wayland session lock that lets the user write toward a chosen word count before unlocking. Preserve the ability to recover the user's writing, especially when changing exit or storage behavior.

## Code map

- `src/main.c` runs the event loop; Wayland callbacks and the keyboard repeat timer drive input.
- `src/app.c` owns phase transitions and input handling. `struct anvi_state` in `include/anvi/app.h` holds the session state.
- `state->document` is the writing buffer; `state->prompt_input` is reused for configuration and finish prompts. Both use `struct anvi_text_buffer` in `include/anvi/text_buffer.h`, whose length and cursor positions are byte offsets into UTF-8 text.
- `src/session_setup.c` creates and destroys the session resources; `src/render.c` draws the current phase.
- Disk storage is planned but not integrated yet. Check the current worktree before changing storage code; `include/anvi/storage.h` may be an in-progress draft.

## Build and checks

Use `meson compile -C build` and `meson test -C build` when the build directory is configured. `meson.build` is the source of truth for dependencies and source files. Pure text-buffer behavior is tested in `tests/text_buffer_test.c`; a real lock-screen run requires a Wayland compositor.
