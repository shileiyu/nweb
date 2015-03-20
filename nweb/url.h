#ifndef NWEB_NSURL_H_
#define NWEB_NSURL_H_

#include "nweb.h"

namespace nweb
{

class URL
{
public:
    URL();

    URL(const char * url);

    URL(const std::string & url);

    ~URL();

    bool IsValid() const;
    
    bool Parse(const char * url);

    bool Parse(const std::string & url);

    std::string Escaped() const;

    void AppendPath(const char * part);

    void ClearPath();

    void AddParam(const char * key, const char * value);

    void RemoveParam(const char * key);

private:
    typedef std::map<std::string, std::string> Dict;
    typedef std::pair<std::string, std::string> DictEntry;

    std::string scheme_;
    std::string host_;
    std::string port_;
    std::string path_;
    std::string username_;
    std::string password_;
    std::string fragment_;
    Dict query_;
};

}

#endif