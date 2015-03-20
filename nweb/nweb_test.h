#ifndef NSWEB_TEST_H_
#define NSWEB_TEST_H_

#include <string>
#include <gtest/gtest.h>

std::string GetLocalPath(const std::string& name);
bool RemoveLocalFile(const std::string& path);

#endif