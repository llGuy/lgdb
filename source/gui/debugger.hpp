#pragma once


#include <thread>
#include <stdint.h>
#include <mutex>
#include <TextEditor.h>

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
};


void debugger_task_start_process(struct shared_t *shared);
void debugger_task_step_over(struct shared_t *shared);
void debugger_task_step_into(struct shared_t *shared);
void debugger_task_step_out(struct shared_t *shared);


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

    bool is_process_running();

    static void open_file_proc(const char *filename, void *obj);

private:

    void handle_debug_event();
    void copy_to_output_buffer(const char *buf);

private:

    static char strdir_buffer[262];

    open_panels_t open_panels_;
    TextEditor editor_;
    char *output_buffer_;
    uint32_t output_buffer_counter_;
    uint32_t output_buffer_max_;
    std::thread loop_thread_;
    shared_t *shared_;
    bool is_running_;

};
