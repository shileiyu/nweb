#include <curl\curl.h>
#include <cares\ares.h>
#include "nweb.h"

namespace nweb
{

void Initialize()
{
    ares_library_init(ARES_LIB_INIT_ALL);
    curl_global_init(CURL_GLOBAL_ALL);
}

void Uninitialize()
{
    curl_global_cleanup();
    ares_library_cleanup();
}

}