#include "nweb_test.h"
#include "http_foreman.h"

namespace
{

TEST(HttpForeman, BigFile)
{
    using namespace nweb;

    static uint32_t tik = 0;
    const char * url = 
    {
        "http://192.168.4.15/apps/dungeon_siege_3.tar"
    };

    uint64_t total;
    auto fr = kResultAgain;
    HttpForeman foreman;

    auto local = GetLocalPath("dungeon_siege_3.tar");
    foreman.SetPrimaryUrl(url);
    foreman.SetFilePath(local.data());
    uint32_t start = GetTickCount();
    while(fr == kResultAgain) 
    {
        fr = foreman.Fetch();
        total = foreman.TotalSize();
        uint32_t tok = GetTickCount();
    }
    uint32_t end = GetTickCount();
    printf("Elapsed : %d\n", (end - start) / 1000);
    EXPECT_EQ(kResultOK, fr);
}

}