// Entry point: boots the MIDlet and runs the MIDP event loop.
#include "classes.h"
#include "platform.h"

#include <cstdio>

static void report(JObject* ex) {
    fprintf(stderr, "uncaught exception: %s\n", ex && ex->cls ? ex->cls->name : "?");
}

static JObject* g_midlet = nullptr;

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    platform_init();
    gc_add_root(&g_midlet);
    try {
        g_midlet = jcreate_main();
        M_javax_microedition_midlet_MIDlet___1_1start__Ljavax_microedition_midlet_MIDlet__V(g_midlet);
    } catch (JObject* ex) {
        report(ex);
        platform_fatal("the game failed to start");
    }
    for (;;) {
        bool running = true;
        try {
            running = M_javax_microedition_lcdui_Display___1_1tick__Ljavax_microedition_midlet_MIDlet__Z(g_midlet) != 0;
        } catch (JObject* ex) {
            report(ex);
        }
        // No translated Java code is on the stack here, so this is a GC safe point.
        gc_maybe_collect();
        if (!running) break;
    }
    try {
        M_javax_microedition_midlet_MIDlet___1_1destroy__Ljavax_microedition_midlet_MIDlet__V(g_midlet);
    } catch (JObject* ex) {
        report(ex);
    }
    platform_shutdown();
    return 0;
}
