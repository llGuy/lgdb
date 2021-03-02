#include "utility.hpp"
#include "debugger.hpp"
#include "file_dialog.hpp"
#include "app_context.hpp"


#include <SDL.h>
#include <imgui.h>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>


void app_context_t::run(const char *cmdline) {
    init();

    while (is_running_) {
        begin_frame();

        /* Update and render */
        ImGuiID main_window = tick_main_window();
        file_dialog_->tick();
        debugger_->tick(main_window);

        end_frame();
    }

    destroy();
}


void app_context_t::init() {
    init_imgui();
    init_window_ctx();

    file_dialog_ = new file_dialog_t;

    debugger_ = new debugger_t;
    debugger_->init();

    is_running_ = 1;
}


void app_context_t::destroy() {

}


void app_context_t::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.9f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.07f, 0.07f, 0.07f, 1.0f);
    ImVec4 *colors = style.Colors;
}


void app_context_t::init_window_ctx() {
    { // SDL
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
            error_and_exit("Failed to initialise SDL\n");

        glsl_version_ = "#version 130";

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        window_ = SDL_CreateWindow("lGdb", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
        gl_ctx_ = SDL_GL_CreateContext(window_);
        SDL_GL_MakeCurrent(window_, gl_ctx_);
        SDL_GL_SetSwapInterval(1);

        if (glewInit() != GLEW_OK)
            error_and_exit("Failed to initialise GLEW\n");
    }

    { // Initialise ImGUI for OpenGL context
        ImGui_ImplSDL2_InitForOpenGL(window_, gl_ctx_);
        ImGui_ImplOpenGL3_Init(glsl_version_);
    }
}


void app_context_t::begin_frame() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);

        if (event.type == SDL_QUIT)
            is_running_ = 0;
        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(window_))
            is_running_ = 0;
    }
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window_);

    ImGui::NewFrame();
}


void app_context_t::end_frame() {
    ImGui::Render();

    ImGuiIO &io = ImGui::GetIO();
    glViewport(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    glm::vec4 clear_color = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(window_);
}


ImGuiID app_context_t::tick_main_window() {
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    uint32_t flags = 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::Begin("Main Window", NULL, flags);

    ImGuiID dock_space_id = ImGui::GetID("HUD_DockSpace");

    ImGui::DockSpace(dock_space_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode);

    tick_menubar();
    ImGui::End();

    tick_debugging_keybindings();

    return dock_space_id;
}


void dummy(const char *file_name) {

}


void app_context_t::tick_menubar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open..", "Ctrl+Shift+O")) {
                file_dialog_->open(debugger_t::open_file_proc, "*\0\0", debugger_);
            }

            if (ImGui::MenuItem("Close")) {
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows")) {
            if (ImGui::MenuItem("Code")) {
            }
            if (ImGui::MenuItem("Watch")) {
            }
            if (ImGui::MenuItem("Dissassembly")) {
            }
            if (ImGui::MenuItem("Memory")) {
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::MenuItem("Start With Debugger", "Alt+Shift+D")) {
            }
            if (ImGui::MenuItem("Start Without Debugger", "Alt+Shift+R")) {
            }
            if (ImGui::MenuItem("Start And Break At Main", "Alt+1")) {
                debugger_->start_and_break_at_main();
            }
            if (ImGui::MenuItem("Step Over", "Alt+D")) {
                debugger_->step_over();
            }
            if (ImGui::MenuItem("Step Into", "Alt+S")) {
            }
            if (ImGui::MenuItem("Step Out", "Alt+W")) {
            }
            if (ImGui::MenuItem("Toggle Breakpoint", "Alt+2")) {
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}


void app_context_t::tick_debugging_keybindings() {
#if 0
    if (ImGui::IsKeyPressed(SDL_SCANCODE_LALT)) {
        if (ImGui::IsKeyReleased(SDL_SCANCODE_1)) {
            debugger_->start_and_break_at_main();
        }

        if (ImGui::IsKeyReleased(SDL_SCANCODE_D)) {
            debugger_->step_over();
        }
    }
#endif
    if (ImGui::IsKeyDown(SDL_SCANCODE_LALT) || ImGui::IsKeyDown(SDL_SCANCODE_RALT)) {

        if (ImGui::IsKeyPressed(SDL_SCANCODE_W)) {
            debugger_->step_out();
        }

        if (ImGui::IsKeyPressed(SDL_SCANCODE_1)) {
            debugger_->start_and_break_at_main();
        }

        if (ImGui::IsKeyPressed(SDL_SCANCODE_S)) {
            debugger_->step_into();
        }

        if (ImGui::IsKeyPressed(SDL_SCANCODE_D)) {
            debugger_->step_over();
        }
    }
}
