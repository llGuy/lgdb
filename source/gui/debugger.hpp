#pragma once


#include <stdint.h>
#include <TextEditor.h>


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


class debugger_t {
public:
    
    debugger_t() = default;

    void init();

    void tick(ImGuiID main);

    void start_with_debugger();
    void start_without_debugger();
    void start_and_break_at_main();
    void step_over();
    void step_in();
    void step_out();

    static void open_file_proc(const char *filename, void *obj);

private:

    void handle_debug_event();

private:

    static char strdir_buffer[262];

    lgdb_process_ctx_t *process_ctx_;
    open_panels_t open_panels_;
    TextEditor editor_;

};
