#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

inline bool exists_file(const char* fn) {
    struct stat buffer;
    return (stat(fn, &buffer) == 0);
}

std::string read_file(const std::string& fn);

void split(const std::string& s, char delim, std::vector<std::string>& v);

void cuda_visible_devices(std::vector<int>& device_ids);
