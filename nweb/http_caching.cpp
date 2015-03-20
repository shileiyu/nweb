#include <assert.h>
#include <curl/curl.h>
#include "block_file.h"
#include "http_caching.h"

namespace nweb
{


class Cache : public HttpResponse
{
public:
    bool Open(const std::string & path)
    {
        Close();

        bool exists = BlockFile::IsFileExist(path.data());
        if (!file_.Open(path.data(), !exists))
            return false;

        if(exists)
        {
            time_t last_write = time(&last_write);
            file_.GetLastWriteTime(last_write);

            tm pt = { 0 };
            if (!gmtime_s(&pt, &last_write))
            {
                char temp[256];
                strftime(temp, sizeof(temp), "%a, %d %b %Y %H:%M:%S GMT", &pt);  
                time_stamp_ = temp;
            }
        }
        return true;
    }

    void Close()
    {
        file_.Close();
        time_stamp_ = "";
        written_size_ = 0;
    }

    const std::string & TimeStamp() const
    {
        return time_stamp_;
    }

    size_t WriteChunk(const void * blob, size_t size)
    {
        if(!file_.Write(blob, size))
            return 0;
        
        written_size_ += size;
        return size;
    }

    uint64_t GetDownloadedSize() const
    {
        return written_size_;
    }

    uint64_t GetTotalSize() const
    {
        return GetContentLength();
    }

    void Update()
    {
        file_.Truncate();
        time_t last_write = time(&last_write);
        if(HasLastModified())
            last_write = GetLastModified();
        file_.SetLastWriteTime(last_write);
    }

private:
    BlockFile file_;
    uint64_t written_size_;
    std::string time_stamp_;
};


HttpCaching::HttpCaching()
{
}

HttpCaching::~HttpCaching()
{
    conn_.fini();
}    

Result HttpCaching::Sync(const std::string & url,
                         const std::string & path,
                         HttpCachingClient * client)
{
    if (url.empty() || path.empty() ) 
        return kResultFailed;

    if(!conn_.init())
        return kResultFailed;

    Cache cache;
    HttpRequest req;

    if (!cache.Open(path))
        return kResultOpenFileFailded;
    auto & lm = cache.TimeStamp();

    req.AddHeader("Connection", "Keep-Alive");        
    req.AddHeader("Accept-Encoding", "gzip, deflate, identity");
    if (!lm.empty()) 
        req.AddHeader("If-Modified-Since", lm.data());

    conn_.Reset();
    conn_.SetUrl(url);
    conn_.SetLowSpeedLimit(128, 32);
    conn_.EnableRedirection(true);
    conn_.SetMaxRedirection(5);
    conn_.SetRequestMethod(HttpRequestMethod::kGet);
    conn_.SetRequest(&req);
    conn_.SetResponse(&cache);
    //do loop fetch
    while(true)
    {
        auto cr = conn_.AsyncPerform();
        if(client)
        {
            HttpCachingProgress progress;
            progress.downloaded_bytes = cache.GetDownloadedSize();
            progress.total_bytes = cache.GetTotalSize();
            if(!client->NotifyProgress(*this, progress))
                return kResultUserAbort;
        }

        if( cr == kConnAgain)
        {
            conn_.Wait(5);
            continue;
        }
        
        if(cr != kConnOK)
            return kResultFailed;

        auto code = cache.GetStatusCode();

        if(code == HttpStatusCode::kNotModified)
        {
            return kResultNotModified;
        }
        else if(code == HttpStatusCode::kOK)
        {
            cache.Update();
            return kResultOK;
        }
        else
        {
            return kResultFailed;
        }
    }
}

}