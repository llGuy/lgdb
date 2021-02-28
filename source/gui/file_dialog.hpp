#pragma once


using file_dialog_open_proc_t = void (*)(const char *path, void *obj);


class file_dialog_t {
public:

    file_dialog_t();
    ~file_dialog_t() = default;

    void open(file_dialog_open_proc_t proc, const char *filter, void *obj);
    void tick();

private:

    file_dialog_open_proc_t proc_;
    const char *filter_;
    void *obj_;

};
