#include "native_ipc_helper_main.h"

int main()
{
    static_assert(sizeof(void*) == 4, "Linux native helper must be i386");
    return re2dj::platform::linux::RunNativeIpcHelper();
}
