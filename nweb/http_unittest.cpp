#include "nweb_test.h"
#include "http.h"

namespace
{
    
class HttpConnectionTestCase : public testing::Test
{
protected:
    virtual void SetUp()
    {
        m_http_handler.init();
    }
    virtual void TearDown()
    {
        m_http_handler.fini();
    }

    class TaskResponse : public nweb::HttpResponse 
    {
    private:
        std::string buffer_;
    public:
        TaskResponse(){}
        virtual ~TaskResponse(){}

        virtual size_t WriteChunk(const void * blob, size_t size)
        {
            buffer_.append(reinterpret_cast<const char*>(blob), size);
            return size;
        }

        const std::string& buffer() {return buffer_;}
    };

    nweb::HttpConnection m_http_handler;
};


TEST_F(HttpConnectionTestCase, SyncDownloadingUrlHead)
{
    using namespace nweb;

    //const char * url = "http://soft.pandoramanager.com/utils/7z920.exe";
    const char * url = "http://www.baidu.com";

    
    m_http_handler.SetUrl(url);
    m_http_handler.SetRequestMethod(nweb::HttpRequestMethod::kHead);
    HttpConnResult result = m_http_handler.Perform();

    ASSERT_EQ(kConnOK, result) << "UrlHead error:" << result;
}

TEST_F(HttpConnectionTestCase, SyncDownloadingUrlFile)
{
    using namespace nweb;

    //const char * url = "http://soft.pandoramanager.com/utils/7z920.exe";
    const char * url = "http://www.baidu.com";
    
    TaskResponse task_response;
    m_http_handler.SetUrl(url);
    m_http_handler.SetRequestMethod(nweb::HttpRequestMethod::kGet);
    m_http_handler.SetResponse(&task_response);
    HttpConnResult result = m_http_handler.Perform();

    ASSERT_EQ(kConnOK, result) << "UrlFile error:" << result;
}

TEST_F(HttpConnectionTestCase, AsyncDownloadingUrlHead)
{
    using namespace nweb;

    //const char * url = "http://soft.pandoramanager.com/utils/7z920.exe";
    const char * url = "http://www.baidu.com";

    TaskResponse task_response;
    m_http_handler.SetUrl(url);
    m_http_handler.SetRequestMethod(nweb::HttpRequestMethod::kHead);
    m_http_handler.SetResponse(&task_response);

    HttpConnResult result = kConnAgain;
    while (kConnAgain == result)
    {
        result = m_http_handler.AsyncPerform();
    }

    ASSERT_EQ(kConnOK, result) << "UrlFile error:" << result;
}

TEST_F(HttpConnectionTestCase, AsyncDownloadingUrlFile)
{
    using namespace nweb;

    //const char * url = "http://soft.pandoramanager.com/utils/7z920.exe";
    const char * url = "http://www.baidu.com";

    TaskResponse task_response;
    m_http_handler.SetUrl(url);
    m_http_handler.SetRequestMethod(nweb::HttpRequestMethod::kGet);
    m_http_handler.SetResponse(&task_response);

    HttpConnResult result = kConnAgain;
    while (kConnAgain == result)
    {
        result = m_http_handler.AsyncPerform();
    }
   
    ASSERT_EQ(kConnOK, result) << "UrlFile error:" << result;
}

}
