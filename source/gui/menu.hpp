#pragma once


class gui_debugger_t;
class popups_t;


class menu_t {
public:

    /* May trigger some debugger events or open popups */
    void tick(gui_debugger_t &debugger, popups_t &popups);

};
