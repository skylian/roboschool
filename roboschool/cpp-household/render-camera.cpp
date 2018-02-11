#define GL_GLEXT_PROTOTYPES

#include "render-simple.h"

using namespace SimpleRender;
using namespace Household;
using std::shared_ptr;
using std::weak_ptr;

void Camera::camera_render(
        const shared_ptr<SimpleRender::Context>& cx,
        bool render_depth,
        bool render_labeling,
        bool print_timing) {
    // change me to see the difference (good values 0 1 2)
	const int RGB_OVERSAMPLING = 1;
	const int AUX_OVERSAMPLING = 2;

	int dw = camera_res_w;
	int dh = camera_res_h;
	int ow = camera_res_w << RGB_OVERSAMPLING;
	int oh = camera_res_h << RGB_OVERSAMPLING;

	//cx->glcx->makeCurrent(cx->surf);
	//CHECK_GL_ERROR;

	if (!viewport || viewport->W!=ow || viewport->H!=oh) {
		viewport.reset(new SimpleRender::ContextViewport(
                    cx, ow, oh, camera_near, camera_far, camera_hfov));
		CHECK_GL_ERROR;
	}

	double rgb_depth_render = 0;
	double rgb_oversample = 0;
	double dep_oversample = 0;
	double metatype_render = 0;
	double metatype_oversample = 0;

	//timer.start();
    // PAINT HERE
	viewport->paint(0, 0, 0, 0, 0, 0, this, 65535, VIEW_CAMERA_BIT, 0);
	CHECK_GL_ERROR;

	//rgb_depth_render = timer.nsecsElapsed()/1000000.0;

	// rgb
	//timer.start();
	camera_rgb.resize(3*camera_res_w*camera_res_h);
	uint8_t tmp[4*ow*oh]; // only 3*ow*oh required, but glReadPixels() somehow 
                          // touches memory after this buffer, demonstrated on 
                          // NVidia 375.20

	glReadPixels(0, 0, ow, oh, GL_RGB, GL_UNSIGNED_BYTE, tmp);

	if (RGB_OVERSAMPLING==0) {
		for (int y=0; y<oh; ++y)
			memcpy(&camera_rgb[y*3*ow], &tmp[(oh-1-y)*3*ow], 3*ow);
	} else {
		uint16_t acc[3*dw*dh];
		memset(acc, 0, sizeof(uint16_t)*3*dw*dh);
		int rs = 3*dw;
		for (int oy=0; oy<oh; oy++) {
			int dy = oy >> RGB_OVERSAMPLING;
			uint8_t* src = &tmp[(oh-1-oy)*3*ow];
			for (int ox=0; ox<ow; ox++) {
				int dx = ox >> RGB_OVERSAMPLING;
				acc[dy*rs + 3*dx + 0] += src[3*ox + 0];
				acc[dy*rs + 3*dx + 1] += src[3*ox + 1];
				acc[dy*rs + 3*dx + 2] += src[3*ox + 2];
			}
		}
		uint8_t* dst = (uint8_t*) &camera_rgb[0];
		for (int t=0; t<3*dw*dh; t++)
			dst[t] = acc[t] >> (RGB_OVERSAMPLING+RGB_OVERSAMPLING);
	}
	//rgb_oversample = timer.nsecsElapsed()/1000000.0;

	// depth from the same render
	int auxw = ow >> AUX_OVERSAMPLING;
	int auxh = oh >> AUX_OVERSAMPLING;
	if (render_depth) {
		//timer.start();
		camera_aux_w = auxw;
		camera_aux_h = auxh;
		camera_depth.resize(sizeof(float)*auxw*auxh);
		camera_depth_mask.resize(auxw*auxh);
		float ftmp[ow*oh];

		glReadPixels(0, 0, ow, oh, GL_DEPTH_COMPONENT, GL_FLOAT, ftmp);

		if (AUX_OVERSAMPLING==0) {
			for (int y=0; y<oh; ++y) {
				float* dst = (float*) &camera_depth[4*y*ow];
				uint8_t* msk = (uint8_t*) &camera_depth_mask[y*ow];
				float* src = &ftmp[(oh-1-y)*ow];
				for (int x=0; x<ow; ++x) {
					float mean = src[x];
                    // change !=0 branch!
					dst[x] = fmax(-1, (mean - 0.9) * (1/0.1));
					msk[x] = 1;
				}
			}
		} else {
			float acc[auxw*auxh];
			float sqr[auxw*auxh];
			memset(acc, 0, sizeof(float)*auxw*auxh);
			memset(sqr, 0, sizeof(float)*auxw*auxh);
			int rs = auxw;
			for (int oy=0; oy<oh; oy++) {
				int dy = oy >> AUX_OVERSAMPLING;
				float* src = &ftmp[(oh-1-oy)*ow];
				for (int ox=0; ox<ow; ox++) {
					int dx = ox >> AUX_OVERSAMPLING;
					acc[dy*rs + dx] += src[ox];
					sqr[dy*rs + dx] += src[ox]*src[ox];
				}
			}
			float* dst = (float*) &camera_depth[0];
			uint8_t* msk = (uint8_t*) &camera_depth_mask[0];
			//float min = +1e10;
			//float max = -1e10;
			for (int t=0; t<auxw*auxh; t++) {
				float mean = acc[t] * 
                        (1.0/(1 << (AUX_OVERSAMPLING+AUX_OVERSAMPLING)));
				float std_squared = sqr[t] * 
                        (1.0 / (1 << (AUX_OVERSAMPLING+AUX_OVERSAMPLING))) - 
                        mean * mean;
				// a==0 very close, almost clipped
				// a==0.8 half of manipulator reach
				// a==1 infinity
				// useful range 0.8 .. 1.0
				dst[t] = fmax(-1, (mean - 0.9) * (1/0.1)); // change ==0 branch!
				//if (dst[t] < min) min = dst[t];
				//if (dst[t] > max) max = dst[t];
                // ignore far away
				msk[t] = (std_squared < 0.000002) && (dst[t] < 0.90);
			}
			//fprintf(stderr, " %0.5f .. %0.5f\n", min, max);
		}
		//dep_oversample = timer.nsecsElapsed()/1000000.0;
	}

	// dense object type presence, from different render (types as color)
	if (render_labeling) {
		//timer.start();

		viewport->paint(0, 0, 0, 0, 0, 0, this, 65535,
                VIEW_METACLASS|VIEW_CAMERA_BIT, 0); // PAINT HERE

		camera_labeling.resize(auxw*auxh);
		camera_labeling_mask.resize(auxw*auxh);
		glReadPixels(0, 0, ow, oh, GL_RGB, GL_UNSIGNED_BYTE, tmp);
		//metatype_render = timer.nsecsElapsed()/1000000.0;

		//timer.start();
		if (AUX_OVERSAMPLING==0) {
			assert(auxw==ow && auxh==oh);
			for (int y=0; y<oh; ++y) {
				uint8_t* dst = (uint8_t*) &camera_labeling[y*auxw];
				uint8_t* msk = (uint8_t*) &camera_labeling_mask[y*auxw];
				for (int x=0; x<ow; ++x) {
					uint8_t t = tmp[(oh-1-y)*3*ow + 3*x + 2];
					dst[x] = t;
					msk[x] = 1;
				}
			}
		} else {
			uint8_t bits_count[auxw*auxh*8];
			memset(bits_count, 0, auxw*auxh*8);
			for (int oy=0; oy<oh; ++oy) {
				int dy = oy >> AUX_OVERSAMPLING;
				uint8_t* dst_count = &bits_count[dy*8*auxw];
				uint8_t* src = &tmp[(oh-1-oy)*3*ow];
				for (int ox=0; ox<ow; ++ox) {
					int dx = ox >> AUX_OVERSAMPLING;
					uint8_t t = src[3*ox + 2];
					dst_count[8*dx + 0] += (t & 0x01) >> 0;
					dst_count[8*dx + 1] += (t & 0x02) >> 1;
					dst_count[8*dx + 2] += (t & 0x04) >> 2;
					dst_count[8*dx + 3] += (t & 0x08) >> 3;
					dst_count[8*dx + 4] += (t & 0x10) >> 4;
					dst_count[8*dx + 5] += (t & 0x20) >> 5;
					dst_count[8*dx + 6] += (t & 0x40) >> 6;
					dst_count[8*dx + 7] += (t & 0x80) >> 7;
				}
			}
			uint8_t* dst = (uint8_t*) &camera_labeling[0];
			uint8_t* msk = (uint8_t*) &camera_labeling_mask[0];
			uint8_t threshold = (1 << (AUX_OVERSAMPLING+AUX_OVERSAMPLING)) * 
                    3 / 3; // 3/3 of subpixels
			uint8_t threshold_item =
                    (1 << (AUX_OVERSAMPLING+AUX_OVERSAMPLING)) * 2 / 3; // 2/3 of subpixels
			for (int t=0; t<auxw*auxh; t++) {
				uint8_t* src_count = &bits_count[8*t];
				dst[t] =
					((src_count[0] >= threshold)<<0) +
					((src_count[1] >= threshold)<<1) +
					((src_count[2] >= threshold)<<2) +
					((src_count[3] >= threshold)<<3) +
					((src_count[4] >= threshold_item)<<4) + // METACLASS_HANDLE
					((src_count[5] >= threshold_item)<<5) + // METACLASS_ITEM
					((src_count[6] >= threshold)<<6) +
					((src_count[7] >= threshold)<<7);
				msk[t] = !!dst[t];
			}
		}
	}

	bool balance_classes = true;
	if (balance_classes && render_labeling) {
		int count_floor = 0;
		int count_walls = 0;
		int count_items = 0;
		uint8_t* msk1 = (uint8_t*) &camera_labeling_mask[0];
		uint8_t* msk2 = (uint8_t*) &camera_depth_mask[0];
		uint8_t* lab = (uint8_t*) &camera_labeling[0];
		for (int t=0; t<auxw*auxh; t++) {
			if (msk1[t]==0) continue;
			count_floor += (lab[t] & METACLASS_FLOOR) ? 1 : 0;
			count_walls += (lab[t] & METACLASS_WALL) ? 1 : 0;
			count_items += (lab[t] &
                    (METACLASS_FURNITURE|METACLASS_HANDLE|METACLASS_ITEM)) ?
                    1 : 0;
		}
        // assume 50 item points always visible (50 of 80x64 == 1%)
		double too_many_walls = double(count_walls + count_floor) /
                (count_items + 50); 
		if (too_many_walls > 1) {
			int threshold = int(RAND_MAX / too_many_walls);
			for (int t=0; t<auxw*auxh; t++) {
				if (msk1[t]==0) continue;
				if (!(lab[t] & (METACLASS_FLOOR|METACLASS_WALL))) continue;
				if (rand() < threshold) continue;
				msk1[t] = 0;
				msk2[t] = 0;
			}
		}
	}

	if (print_timing) fprintf(stderr,
		"rgb_depth_render=%6.2lfms  "
		"rgb_oversample=%6.2lfms  "
		"dep_oversample=%6.2lfms  "
		"metatype_render=%6.2lfms  "
		"metatype_oversample=%6.2lfms\n",
		rgb_depth_render,
		rgb_oversample,
		dep_oversample,
		metatype_render,
		metatype_oversample
		);
}


