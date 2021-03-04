#pragma once


class file_dialog_t;
class debugger_t;

struct SDL_Window;
struct ImFont;

using SDL_GLContext = void *;
using ImGuiID = unsigned int;


class app_context_t {
public:

    /* To have more control over initialisation, initialisation happens at init */
    app_context_t() = default;
    /* To have more control over deinitialisation, deinitialisation happens at destroy */
    ~app_context_t() = default;

    void run(const char *cmdline);

private:

    void init();
    void destroy();

    void init_imgui();
    void init_window_ctx();

    void begin_frame();
    void end_frame();

    ImGuiID tick_main_window();
    void tick_menubar();
    void tick_debugging_keybindings();

private:

    SDL_Window *window_;
    SDL_GLContext gl_ctx_;
    /* Needed for ImGUI */
    const char *glsl_version_;
    bool is_running_;

    file_dialog_t *file_dialog_;
    debugger_t *debugger_;

    ImFont *font_;

};
