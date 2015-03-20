#ifndef NWEB_RESOLVER_H_
#define NWEB_RESOLVER_H_

#include <stdint.h>
#include <unordered_set>
#include <unordered_map>
#include <string>

struct ares_channeldata;
typedef ares_channeldata * ares_channel;

namespace nweb
{

class Resolver
{
private:
    typedef std::unordered_set<uint32_t> Addresses;
    typedef std::unordered_map<std::string, Addresses> Hosts;
public:
    Resolver();
    ~Resolver();

    void Perform();

    /*
    Issue a query, you can retrive the results by calling GetAddressList later.
    */
    void QueryHostByName(const char * name);

    void InsertRecord(const char * name, uint32_t address);

    size_t GetAddressCount(const char * name);
    size_t GetAddressList(const char * name, uint32_t * addr_list, size_t size);
private:
    Resolver(const Resolver &);
    Resolver & operator=(const Resolver &);

    bool LazyInitialize();
    void Cleanup();
private:
    ares_channel channel_;
    Hosts hosts_;
};

}

#endif