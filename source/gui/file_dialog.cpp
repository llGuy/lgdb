#include "file_dialog.hpp"


#include <SDL.h>
#include <stdio.h>
#include <assert.h>
#include <Windows.h>


file_dialog_t::file_dialog_t() 
    : proc_(NULL), filter_(NULL) {
    /* Empty for now */
}


void file_dialog_t::open(file_dialog_open_proc_t proc, const char *filter) {
    proc_ = proc;
    filter_ = filter;
}


void file_dialog_t::tick() {
    if (proc_) {
        OPENFILENAMEA info = {};
        char file_name[260] = {};
        info.lStructSize = sizeof(OPENFILENAMEA);
        // info.hwndOwner
        info.lpstrFile = file_name;
        info.nMaxFile = sizeof(file_name);
        info.lpstrFilter = filter_;
        info.nFilterIndex = 1;
        info.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&info) == TRUE) {
            // Call the procedure
            proc_(info.lpstrFile);
            proc_ = NULL;
        }
        else {
            proc_ = NULL;
        }
    }
}