
#define APPLICATION_IMPLEMENTATION
#include "include/application.h"

#include "genmapapp.h"

static int counter = 0;

void *operator new(std::size_t sz)
{
    if (sz == 0)
        ++sz; // avoid std::malloc(0) which may return nullptr on success

    counter++;

    if (void *ptr = std::malloc(sz))
        return ptr;

    throw std::bad_alloc{}; // required by [new.delete.single]/3
}

int main(
    int argc,
    char *argv[])
{
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug

    GenMapApp t;

    if (argc > 2)
    {
        spdlog::debug("loading map {1} from {0}", argv[1], argv[2]);
        t.SetFilename(argv[1], argv[2]);
    }
    else if (argc > 1)
    {
        spdlog::debug("loading map {0}", argv[1]);
    }

    auto result = Application::Run<GenMapApp>(t);

    std::cout << counter << " allocations" << std::endl;

    return result;
}
