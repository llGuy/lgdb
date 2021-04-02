#pragma once


#include <thread>
#include <stdint.h>
#include <mutex>
#include <TextEditor.h>
#include <unordered_map>
#include <fstream>

#include "swap.hpp"
#include "variable.hpp"

extern "C" {
#include <lgdb_context.h>
}


typedef struct lgdb_process_ctx lgdb_process_ctx_t;


struct open_panels_t {
    union {
        struct {
            uint8_t code : 1;
            uint8_t watch : 1;
            uint8_t dissassembly : 1;
            uint8_t memory : 1;
            uint8_t breakpoints : 1;
            uint8_t output : 1;
            uint8_t call_stack : 1;
            uint8_t fun_with_bits : 1;
        };
        uint8_t bits;
    };
};


#define MAX_TASK_COUNT 20


using debugger_task_t = void (*)(struct shared_t *shared);


struct shared_t {
    bool processing_events;
    std::mutex ctx_mutex;

    uint32_t pending_task_count;
    debugger_task_t tasks[MAX_TASK_COUNT];

    lgdb_process_ctx_t *ctx;

    union {
        struct {
            const char *file_name;
            int32_t line_number;
        } toggle_breakpoint;
    } task_parameters;
};


void debugger_task_start_process(struct shared_t *shared);
void debugger_task_step_over(struct shared_t *shared);
void debugger_task_step_into(struct shared_t *shared);
void debugger_task_step_out(struct shared_t *shared);
void debugger_task_continue(struct shared_t *shared);
void debugger_task_toggle_breakpoint(struct shared_t *shared);


struct source_file_t {
    uint32_t file_path_hash;
    std::ifstream stream;
    // std::string contents;
    std::string file_name;
    TextEditor editor;
};


struct call_stack_frame_t {
    uint64_t addr;
    std::string function_name;
    std::string file_name;
    std::string module_name;
    int32_t line_number;
};


/* May be a scope, or just a single variable, etc... */
struct watch_frame_t {
    /* 
       If this is allocated in the variable_info_allocator:
       - All the lgdb_symbol_t structures
       - The sym name to ptr table
    */

    void *start_in_variable_info_allocator;

    variable_info_t **symbol_ptr_pool_start;

    uint64_t stack_frame;
    uint32_t var_count;

    struct {
        uint32_t is_stack_frame : 1;
    } flags;
};


class popup_t {
public:
    virtual ~popup_t() {};
    virtual bool update(
        lgdb_process_ctx_t *ctx,
        lgdb_linear_allocator_t *info_alloc,
        var_swapchain_t *copy_alloc,
        std::vector<variable_info_t *> *modified_vars) = 0;
};


class popup_view_count_t : public popup_t {
public:
    popup_view_count_t(variable_info_t *variable_info) 
        : info_(variable_info), popup_name_("Enter View Count"), open_popup_(1) {
    }

    ~popup_view_count_t() = default;

    bool update(
        lgdb_process_ctx_t *ctx,
        lgdb_linear_allocator_t *info_alloc,
        var_swapchain_t *copy_alloc,
        std::vector<variable_info_t *> *modified_vars) override {
        if (open_popup_) {
            ImGui::OpenPopup(popup_name_);
            open_popup_ = 0;
        }

        if (ImGui::BeginPopupModal(popup_name_)) {
            char count_str[10] = {};

            if (ImGui::InputText("Count", count_str, 4, ImGuiInputTextFlags_EnterReturnsTrue)) {
                info_->requested = strtol(count_str, NULL, 10);
                info_->deep_sync(ctx, info_alloc, copy_alloc, modified_vars);

                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();

                return 1;
            }

            ImGui::SameLine();

            if (ImGui::Button("OK")) {
                info_->requested = strtol(count_str, NULL, 10);
                info_->deep_sync(ctx, info_alloc, copy_alloc, modified_vars);
                ImGui::CloseCurrentPopup();

                return 1;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        return 0;
    }
private:
    const char *popup_name_;
    variable_info_t *info_;
    bool open_popup_;
};


class debugger_t {
public:
    
    debugger_t() = default;

    void init();

    void tick(ImGuiID main);

    void start_with_debugger();
    void start_without_debugger();
    void start_and_break_at_main();
    void step_over();
    void step_into();
    void step_out();
    void toggle_breakpoint(const char *file_name, int line);
    void continue_process();

    bool is_process_running();

    static void open_file_proc(const char *filename, void *obj);
    static void update_local_symbol(lgdb_process_ctx_t *ctx, const char *name, lgdb_symbol_t *sym, void *obj);
    static void update_call_stack(lgdb_process_ctx_t *ctx, void *obj, lgdb_call_stack_frame_t *frame);

private:

    /* Returns true if uncollapsed */
    bool render_composed_var_row(const char *name, const char *type, uint32_t size);
    bool render_symbol_base_type_data(const char *name, const char *type_name, void *address, uint32_t size, lgdb_symbol_type_t *type);
    bool render_symbol_type_data(
        variable_info_t *info,
        const char *name,
        const char *type_name,
        void *address,
        uint32_t size,
        lgdb_symbol_type_t *type);
    // Updates all child variables too if needed
    void handle_debug_event();
    void copy_to_output_buffer(const char *buf);
    source_file_t *update_text_editor_file(const char *file_name);
    void restore_current_file();
    void update_locals(lgdb_process_ctx_t *ctx);
    void update_call_stack();
    void clear_modified_variables();

private:

    static char strdir_buffer[262];

    // TextEditor editor_;
    open_panels_t open_panels_;
    char *output_buffer_;
    uint32_t output_buffer_counter_;
    uint32_t output_buffer_max_;
    std::thread loop_thread_;
    shared_t *shared_;
    bool is_running_;
    std::unordered_map<uint32_t, uint32_t> source_file_map_;
    std::vector<source_file_t *> source_files_;
    int32_t current_src_file_idx_;
    int32_t current_src_file_hash_;

    uint64_t current_stack_frame_;
    int32_t current_watch_frame_idx_;
    std::vector<watch_frame_t> watch_frames_;

    std::unordered_map<uint32_t, variable_info_t *> sym_idx_to_ptr;

    /* 
       A contiguous list of pointers to variable information 
       These pointers point to somewhere in the variable_info_allocator_
    */
    variable_info_t **symbol_ptr_pool_;
    uint32_t symbol_ptr_count_;
    uint32_t max_symbol_ptr_count_;
    lgdb_linear_allocator_t variable_info_allocator_;

    /* Contains a copy of the variables */
    var_swapchain_t var_copy_;

    bool creating_new_frame_;
    bool is_process_suspended_;

    std::vector<call_stack_frame_t> call_stack_;
    bool changed_frame_;

    std::vector<popup_t *> popups_;
    std::vector<variable_info_t *> modified_vars_;

public:

    ImGuiID dock;

};
