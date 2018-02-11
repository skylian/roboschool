#!/usr/bin/python

import sys
import os

if __name__ == "__main__":
    include_deps = [d for d in sys.argv[1:] if os.path.isdir(d)]
    lib_deps = [d for d in sys.argv[1:] if os.path.isfile(d)]
    assert len(include_deps) + len(lib_deps) == len(sys.argv) - 1
    include_flag = " ".join(["-I" + d for d in include_deps])
    lib_flag = " ".join([d for d in lib_deps])

    os.system('make INCLUDE_FLAGS="%s" LIB_FLAGS="%s" -j4 dirs ../libroboschool.so'
        % (include_flag, lib_flag))
