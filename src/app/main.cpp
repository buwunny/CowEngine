#include "app/Application.hpp"

int main()
{
    Application app;
#ifdef __EMSCRIPTEN__
    app_set_global(&app);
#endif
    app.init();
#ifdef __EMSCRIPTEN__
    app_run_main_loop();
#else
    app.runDesktop();
#endif
    return 0;
}