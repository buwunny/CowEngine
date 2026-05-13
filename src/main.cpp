#include "Application.hpp"

int main()
{
    Application app;
    app.init();
#ifdef __EMSCRIPTEN__
    // Register global app pointer for the emscripten main loop
    app_set_global(&app);
    app_run_main_loop();
#else
    app.runDesktop();
#endif
    return 0;
}