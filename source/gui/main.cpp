#include "app_context.hpp"


#include <Windows.h>


#if 1
int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd) {
#else
int main() {
#endif
    app_context_t ctx = {};
    ctx.run(NULL);
}
