#include <stdio.h>
#include "Halide.h"

using namespace Halide;

// This test exercises the ability to override halide_get_library_symbol (etc)
// when using JIT code; to do so, it compiles & calls a simple pipeline
// using an OpenCL schedule, since that is known to use these calls
// in a (reasonably) well-defined way and is unlikely to change a great deal 
// in the near future; additionally, it doesn't require a particular
// feature in LLVM (unlike, say, Hexagon).

namespace {

int load_library_calls = 0;
int get_library_symbol_calls = 0;

void my_error_handler(void* u, const char *msg) {
    if (!strstr(msg, "OpenCL API not found")) {
        fprintf(stderr, "Saw unexpected error: %s\n", msg);
        exit(-1);
    }
    printf("Saw expected error: %s\n", msg);
    if (load_library_calls == 0 || get_library_symbol_calls == 0) {
        fprintf(stderr, "Should have seen load_library and get_library_symbol calls!\n");
        exit(-1);
    }
    printf("Success!\n");
    exit(0);
}

void *my_get_symbol_impl(const char *name) {
    fprintf(stderr, "Saw unexpected call: get_symbol(%s)\n", name);
    exit(-1);
}

void *my_load_library_impl(const char *name) {
    load_library_calls++;
    if (!strstr(name, "OpenCL") && !strstr(name, "opencl")) {
        fprintf(stderr, "Saw unexpected call: load_library(%s)\n", name);
        exit(-1);
    }
    printf("Saw load_library: %s\n", name);
    return nullptr;
}

void *my_get_library_symbol_impl(void *lib, const char *name) {
    get_library_symbol_calls++;
    if (lib != nullptr || strcmp(name, "clGetPlatformIDs") != 0) {
        fprintf(stderr, "Saw unexpected call: get_library_symbol(%p, %s)\n", lib, name);
        exit(-1);
    }
    printf("Saw get_library_symbol: %s\n", name);
    return nullptr;
}

}

int main(int argc, char **argv) {
    // These calls are only available for AOT-compiled code:
    //
    //   halide_set_custom_get_symbol(my_get_symbol_impl);
    //   halide_set_custom_load_library(my_load_library_impl);
    //   halide_set_custom_get_library_symbol(my_get_library_symbol_impl);
    //
    // For JIT code, we must use JITSharedRuntime::set_default_handlers().

    Internal::JITHandlers handlers;
    handlers.custom_get_symbol = my_get_symbol_impl;
    handlers.custom_load_library = my_load_library_impl;
    handlers.custom_get_library_symbol = my_get_library_symbol_impl;
    Internal::JITSharedRuntime::set_default_handlers(handlers);

    Var x, y, xi, yi;
    Func f;
    f(x, y) = cast<int32_t>(x + y);
    Target target = get_jit_target_from_environment().with_feature(Target::OpenCL);
    f.gpu_tile(x, y, xi, yi, 8, 8, TailStrategy::Auto, DeviceAPI::OpenCL);
    f.set_error_handler(my_error_handler);

    Buffer<int32_t> out = f.realize(64, 64, target);

    fprintf(stderr, "Should not get here.\n");
    return -1;
}
