#ifndef NWEB_HTTP_CACHING_H_
#define NWEB_HTTP_CACHING_H_

#include "http.h"


namespace nweb
{

class HttpCaching;

struct HttpCachingProgress
{
    uint64_t downloaded_bytes;
    uint64_t total_bytes;
};

class HttpCachingClient
{
public:
    virtual bool NotifyProgress(HttpCaching&, HttpCachingProgress&) = 0;
};

class HttpCaching
{
public:
    HttpCaching();
    ~HttpCaching();

    Result Sync(const std::string & url,
                const std::string & path,
                HttpCachingClient * client);

private:
    HttpConnection conn_;
};



}

#endif