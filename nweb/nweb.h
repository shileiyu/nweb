#ifndef NWEB_DEFINE_H_
#define NWEB_DEFINE_H_

#include <assert.h>
#include <stdint.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>



#ifdef _MSC_VER
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif


namespace nweb
{

void Initialize();

void Uninitialize();

enum Result
{
    kResultOK                   = 0,
    kResultAgain                = 1,
    kResultNotModified          = 2,
    kResultFailed               = -1,
    kResultUserAbort             = -2,
    kResultFileSizeUnknown      = -3,
    kResultFileSizeMismatch     = -4,
    kResultOpenFileFailded      = -5,
    kResultSaveBlockFailded     = -6,
};

}

#endif