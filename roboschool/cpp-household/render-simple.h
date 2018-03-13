#pragma once

#include <ios>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "gl_context.h"
#include "household.h"
#include "shader.h"

struct aiMesh;

namespace ssao {
class ScreenSpaceAmbientOcclusion;
}

// optimizes blur, by storing depth along with ssao calculation
// avoids accessing two different textures
#define USE_AO_SPECIALBLUR  1

namespace SimpleRender {

extern void primitives_to_mesh(
        const std::shared_ptr<Household::ShapeDetailLevels>& m,
        int want_detail,
        int shape_n);

enum {
	VIEW_CAMERA_BIT      = 0x0001,
	VIEW_LINES           = 0x0001,
	VIEW_DEBUG_LINES     = 0x0002,
	VIEW_COLLISION_SHAPE = 0x0004,
	VIEW_METACLASS       = 0x0010,
	VIEW_NO_HUD          = 0x1000,
	VIEW_NO_CAPTIONS     = 0x2000,
};

struct Texture {
	GLuint handle;
	Texture() { glGenTextures(1, &handle); }
	~Texture() { glDeleteTextures(1, &handle); }
};

struct Framebuffer {
	GLuint handle;
	Framebuffer() { glGenFramebuffers(1, &handle); }
	~Framebuffer() { glDeleteFramebuffers(1, &handle); }
};

struct Buffer {
	GLuint handle;
	Buffer() { glGenBuffers(1, &handle); }
	~Buffer() { glDeleteBuffers(1, &handle); }
};

struct VAO {
	GLuint handle;
	VAO() { glGenVertexArrays(1, &handle); }
	~VAO() { glDeleteVertexArrays(1, &handle); }
};

const int AO_RANDOMTEX_SIZE = 4;
const int MAX_SAMPLES = 8;
const int NUM_MRT = 8;
const int HBAO_RANDOM_SIZE = AO_RANDOMTEX_SIZE;
const int HBAO_RANDOM_ELEMENTS = HBAO_RANDOM_SIZE*HBAO_RANDOM_SIZE;

struct Context {
	Context(const std::shared_ptr<Household::World>& world);
	~Context();

	bool slowmo = false;

    enum {
        ATTR_N_VERTEX,
        ATTR_N_NORMAL,
        ATTR_N_TEXCOORD,
    };

	std::weak_ptr<Household::World> weak_world;

	GLContext* glcx = 0;
	std::vector<std::shared_ptr<Texture>> textures;

	int loc_input_matrix_modelview_inverse_transpose;
	int loc_input_matrix_modelview;
	int loc_enable_texture;
	int loc_texture;
	int loc_uni_color;
	int loc_multiply_color;
	std::shared_ptr<GLShaderProgram> program_tex;

	std::shared_ptr<GLShaderProgram> program_displaytex;

	int loc_clipInfo;
	std::shared_ptr<GLShaderProgram> program_depth_linearize;


	GLint loc_RadiusToScreen;
	GLint loc_R2;             // 1/radius
	GLint loc_NegInvR2;       // radius * radius
	GLint loc_NDotVBias;
	GLint loc_InvFullResolution;
	GLint loc_InvQuarterResolution;
	GLint loc_AOMultiplier;
	GLint loc_PowExponent;
	GLint loc_projInfo;
	GLint loc_projScale;
	GLint loc_projOrtho;
	GLint loc_float2Offsets;
	GLint loc_jitters;
	GLint loc_texLinearDepth;
	GLint loc_texRandom;

	bool ssao_enable = true;
	std::shared_ptr<GLShaderProgram> program_hbao_calc;
	std::shared_ptr<GLShaderProgram> program_calc_blur;

	std::list<std::shared_ptr<VAO>>    allocated_vaos;
	std::list<std::shared_ptr<Buffer>> allocated_buffers;

	float pure_color_opacity = 1.0;

	int a=0, b=0, c=0;

	bool initialized = false;

	void load_missing_textures();
	bool need_load_missing_textures = true;

	int cached_bind_texture(const std::string& image_fn);
	std::map<std::string, int> bind_cache;

	std::shared_ptr<struct UsefulStuff> useful = 0;
	void initGL();

	bool _hbao_init();
	std::shared_ptr<Texture> hbao_random;
	std::shared_ptr<Texture> hbao_randomview[MAX_SAMPLES];

	std::shared_ptr<VAO> ruler_vao;
	std::shared_ptr<Buffer> ruler_vertexes;

	void _shape_to_vao(const std::shared_ptr<Household::Shape>& shape);
	void _generate_ruler_vao();
};

class ContextViewport {
public:
	std::shared_ptr<Context> cx;
	int visible_object_count;

	int W, H;
	double side, near, far, hfov;
    glm::mat4 modelview;
	glm::mat4 modelview_inverse_transpose;

	int  ssao_debug = 0;
	bool ortho = false;
	bool blur = false;
	int samples = 1;

	// heavy shadow
	//float ssao_radius    = 1.6; // search for "ssao.params"
	//float ssao_intensity = 3.0;
	//float ssao_bias      = 0.70;

	// light
	float ssao_radius    = 0.7;
	float ssao_intensity = 0.9;
	float ssao_bias      = 0.8;

	ContextViewport(const std::shared_ptr<Context>& cx,
                    int W, int H,
                    double near, double far, double hfov);

	std::shared_ptr<Framebuffer> fbuf_scene;
	std::shared_ptr<Framebuffer> fbuf_depthlinear;
	std::shared_ptr<Framebuffer> fbuf_viewnormal;
	std::shared_ptr<Framebuffer> fbuf_hbao_calc;
	std::shared_ptr<Framebuffer> fbuf_hbao2_deinterleave;
	std::shared_ptr<Framebuffer> fbuf_hbao2_calc;

	std::shared_ptr<Texture> tex_color;
	std::shared_ptr<Texture> tex_depthstencil;
	std::shared_ptr<Texture> tex_depthlinear;
	std::shared_ptr<Texture> tex_viewnormal;
	std::shared_ptr<Texture> hbao_result;
	std::shared_ptr<Texture> hbao_blur;
	std::shared_ptr<Texture> hbao2_deptharray;
	std::shared_ptr<Texture> hbao2_depthview[HBAO_RANDOM_ELEMENTS];
	std::shared_ptr<Texture> hbao2_resultarray;

    void paint(float user_x,
               float user_y,
               float user_z,
               float wheel,
               float zrot,
               float xrot,
               Household::Camera* camera,
               int floor_visible,
               uint32_t view_options,
               float ruler_size);
private:
	void _depthlinear_init();
	void _depthlinear_paint(int sample_idx);
	void _hbao_prepare(const float* proj_matrix);
	void _ssao_run(int sampleIdx);
	void _texture_paint(GLuint h);
	int  _objects_loop(int floor_visible, uint32_t view_options);
	void _render_single_object(
            const std::shared_ptr<Household::ShapeDetailLevels>& m,
            uint32_t f,
            int detail,
            const glm::mat4& at_pos);
};

void opengl_init(const std::shared_ptr<Household::World>& wref, int h, int w);

} // namespace
