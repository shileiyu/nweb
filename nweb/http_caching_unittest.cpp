#include "nweb_test.h"
#include "http_caching.h"

namespace
{

class HttpCachingUnitTest : public testing::Test
{
protected:
    virtual void SetUp()
    {
    }
    virtual void TearDown()
    {
    }

    class MyCallback : public nweb::HttpCachingClient
    {
    public:
        MyCallback():switch_on_(false) {}

        bool NotifyProgress(
            nweb::HttpCaching & client,
            nweb::HttpCachingProgress & progress
        ) {
            static int counter = 0;
            if (++counter%1000 == 0)
            {
                printf("downloading progress: (all: %d, now: %d)\n", 
                       progress.total_bytes, progress.downloaded_bytes);
                return !switch_on_;
            }

            return true;
        }

        void SwitchOn()
        {
            switch_on_ = true;
        }
    private:
        uint32_t total_size_;
        uint32_t downloaded_size_;

        bool switch_on_;
    };

    nweb::HttpCaching http_caching_;
};

TEST_F(HttpCachingUnitTest, FirstTimeDownloadFileWithManualCancel)
{
    using namespace nweb;


    const char * url = "http://soft.pandoramanager.com/dev/VC-Compiler-KB2519277.exe";
    std::string path = GetLocalPath("test.exe");
    MyCallback cb; cb.SwitchOn();
    RemoveLocalFile(path);
    ASSERT_EQ(http_caching_.Sync(url, path.c_str(), &cb), kResultUserAbort)
        << "Oh, this is a fatal mistake!";
}

TEST_F(HttpCachingUnitTest, FirstTimeDownloadFile)
{
    using namespace nweb;

    //const char * url = "http://api.games.pandoramanager.com/app_info.php";
    const char * url = "http://soft.pandoramanager.com/dev/VC-Compiler-KB2519277.exe";
    std::string path = GetLocalPath("test.exe");

    RemoveLocalFile(path);
    MyCallback cb;
    ASSERT_EQ(http_caching_.Sync(url, path.c_str(), &cb), kResultOK);
}

 TEST_F(HttpCachingUnitTest, SecondTimeDownloadFile)
 {
     using namespace nweb;
  
     const char * url = "http://soft.pandoramanager.com/dev/VC-Compiler-KB2519277.exe";
     //const char * url = "http://api.games.pandoramanager.com/app_info.php";
     std::string path = GetLocalPath("test.exe");
    
     MyCallback cb;
     ASSERT_EQ(http_caching_.Sync(url, path.c_str(), &cb), kResultNotModified);
 }

}
