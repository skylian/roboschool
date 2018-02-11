#pragma once

#include <cstring>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include "mat.h"

inline bool exists_file(const char* fn) {
    struct stat buffer;
    return (stat(fn, &buffer) == 0);
}

std::string read_file(const std::string& fn);

Matuc read_img(const char* fname);

