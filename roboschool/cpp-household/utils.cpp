#include "utils.h"

std::string read_file(const std::string& fn) {
    FILE* f = fopen(fn.c_str(), "rt");
    if (!f) {
        throw std::runtime_error("cannot open '" + fn + "' with mode 'rt': "
                                 + std::strerror(errno));
    }
    off_t ret = fseek(f, 0, SEEK_END);
    if (ret == (off_t) - 1) {
        fclose(f);
        throw std::runtime_error("cannot stat '" + fn + "': " + std::strerror(errno));
    }
    uint32_t file_size = (uint32_t) ftell(f); // ftell returns long int
    fseek(f, 0, SEEK_SET);
    std::string str;
    if (file_size == 0) {
        fclose(f);
        return str;
    }
    str.resize(file_size);
    try {
        int r = (int) fread(&str[0], file_size, 1, f);
        if (r==0) {
            throw std::runtime_error("cannot read from '" + fn + "', eof");
        }
        if (r!=1) {
            throw std::runtime_error("cannot read from '" + fn + "': "
                                     + std::strerror(errno));
        }
        fclose(f);
    } catch (...) {
        fclose(f);
        throw;
    }
    return str;
}
