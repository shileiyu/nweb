#include "nweb_test.h"
#include "url.h"

TEST(URL, PARSE)
{
    using namespace nweb;

    const char * kUrl = "http://www.pandoramamanger.com/api/app_latest_version"
                        "?app_id=100&carbinet=master";
    URL u;
    u.Parse(kUrl);
    auto escaped = u.Escaped();
    EXPECT_TRUE(kUrl == escaped);
}