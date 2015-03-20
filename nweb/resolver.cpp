#include <cares\ares.h>
#include "resolver.h"

namespace nweb
{

namespace
{

void HostCallback(void * arg, int status, int timeouts, hostent * hostent)
{
    if(arg == 0)
        return;

    auto resolver = reinterpret_cast<Resolver *>(arg);

    if( hostent && 
        hostent->h_name)
    {
        auto addr_list = reinterpret_cast<uint32_t **>(hostent->h_addr_list);
        auto alias_list = reinterpret_cast<char **>(hostent->h_aliases);
        for(int i = 0; addr_list[i] != 0; ++i)
        {
            auto address = ntohl(*addr_list[i]);
            resolver->InsertRecord(hostent->h_name, address);
            for(int j = 0; alias_list[j] != 0; ++j)
                resolver->InsertRecord(alias_list[j], address);
        }
    }
}

}


Resolver::Resolver()
    : channel_(0)
{
}

Resolver::~Resolver()
{
    Cleanup();
}

void Resolver::Perform()
{
    if(!LazyInitialize())
        return;

    int fd_count = 0;
    fd_set readers, writers;

    FD_ZERO(&readers);
    FD_ZERO(&writers);
    fd_count = ares_fds(channel_, &readers, &writers);
    if (fd_count == 0)
        return;
    ares_process(channel_, &readers, &writers);
}

void Resolver::QueryHostByName(const char * name)
{
    if(!LazyInitialize())
        return;
    ares_gethostbyname(channel_, name, AF_INET, HostCallback, this);
}

void Resolver::InsertRecord(const char * name, uint32_t address)
{
    hosts_[name].insert(address);
}

size_t Resolver::GetAddressCount(const char * name)
{
    if(name == 0)
        return 0;

    return hosts_[name].size();
}

size_t Resolver::GetAddressList(const char * name, 
                                uint32_t * addr_list, 
                                size_t size)
{
    if(name == 0 || addr_list == 0 || size == 0)
        return 0;

    size_t addr_index = 0;

    auto & addr_set = hosts_[name];

    for(auto iter = addr_set.begin(); iter != addr_set.end(); ++iter)
    {
        auto addr = *iter;
        if(size < sizeof(uint32_t))
            break;
        addr_list[addr_index++] = addr;
    }

    return addr_index * sizeof(uint32_t);
}

bool Resolver::LazyInitialize()
{
    if(channel_ == 0)
        if(ares_init(&channel_) != ARES_SUCCESS)
            return false;
    return true;
}

void Resolver::Cleanup()
{
    if(channel_)
    {
        ares_destroy(channel_);
        channel_ = 0;
    }
}

}