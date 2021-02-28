#include "app_context.hpp"


#include <Windows.h>


#if 0
int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd) {
#endif
int main() {
    app_context_t ctx = {};
    ctx.run(NULL);
}
