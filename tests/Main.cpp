#define DOCTEST_CONFIG_IMPLEMENT
#include <cstdlib>
#include <ctime>

#include "doctest/doctest.h"

int main(int argc, char** argv) {
    // the tests read and write local times: they are run in one zone, whatever the machine is set to
#ifdef _WIN32
    _putenv_s("TZ", "UTC");
    _tzset();
#else
    setenv("TZ", "UTC", 1);
    tzset();
#endif
    doctest::Context context(argc, argv);
    return context.run();
}
