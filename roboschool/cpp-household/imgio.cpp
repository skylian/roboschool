// Copyright 2017-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the license found in the
// LICENSE file in the root directory of this source tree.
//File: imgio.cc


#include <cstdlib>
#include <vector>

#define cimg_display 0
#define cimg_use_jpeg
#define cimg_use_png
#include "CImg.h"

#include "utils.h"

using namespace cimg_library;
using namespace std;

Matuc read_img(const char* fname) {
	if (!exists_file(fname)) {
		fprintf(stderr, "File \"%s\" not exists!", fname);
        return Matuc();
    }

    CImg<unsigned char> img(fname);
    int channel = img.spectrum();
	assert(channel == 3 || channel == 1 || channel == 4);

	Matuc mat(img.height(), img.width(), (channel == 1 ? 3: channel));
	assert(mat.rows() > 1 && mat.cols() > 1);

    for (auto i = 0; i < mat.rows(); ++i) {
        for (auto j = 0; j < mat.cols(); ++j) {
            if (channel == 1) {
                mat.at(i,j,0) = img(j,i,0);
                mat.at(i,j,1) = img(j,i,0);
                mat.at(i,j,2) = img(j,i,0);
            } else {
                for (auto k = 0; k < channel; ++k) {
                    mat.at(i,j,k) = img(j,i,k);
                }
            }
        }
    }

    return mat;
}
