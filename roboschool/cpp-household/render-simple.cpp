#define GL_GLEXT_PROTOTYPES
#include <glm/gtc/type_ptr.hpp> 
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "render-simple.h"
#include "utils.h"


// OpenGL compatibility:
// https://en.wikipedia.org/wiki/Intel_HD_and_Iris_Graphics#Capabilities
// https://developer.apple.com/opengl/capabilities/

// Mac Intel GPU:
// sysctl -n machdep.cpu.brand_string
// google for processor id

using smart_pointer::shared_ptr;
using smart_pointer::weak_ptr;

std::string glsl_path = "roboschool/cpp-household/glsl"; // outside namespace

namespace SimpleRender {

using namespace Household;

static float line_vertex[] = {
+1,+1,0, -1,+1,0,
-1,+1,0, -1,-1,0,
-1,-1,0, +1,-1,0,
+1,-1,0, +1,+1,0,
-2,0,0, +2,0,0,
0,-2,0, 0,+2,0,

+1,+1,-1, +1,+1,+1,
-1,+1,-1, -1,+1,+1,
-1,-1,-1, -1,-1,+1,
+1,-1,-1, +1,-1,+1,
};

static glm::mat4 btTransform_to_mat4(const btTransform& t) {
    btScalar m[16];
    t.getOpenGLMatrix(m);
    return glm::make_mat4(m);
}

// this helps with btScalar
static void glMultMatrix(const float* m) {
    glMultMatrixf(m);
}  

static void glMultMatrix(const double* m) {
    glMultMatrixd(m);
}

Context::Context(const shared_ptr<Household::World>& world) : 
        weak_world(world),
        program_tex(nullptr),
        program_displaytex(nullptr),
        program_depth_linearize(nullptr),
        program_hbao_calc(nullptr),
        program_calc_blur(nullptr) {
}

Context::~Context() {
	//glcx->makeCurrent(surf);
	// destructors here
}

int Context::cached_bind_texture(const std::string& image_fn) {
	auto f = bind_cache.find(image_fn);
	if (f != bind_cache.end()) {
		return f->second;
    }
    int b = 0;
    cv::Mat img = imread(image_fn, cv::IMREAD_COLOR);
    if (img.empty()) {
		fprintf(stderr, "cannot read image '%s", image_fn.c_str());
    } else {
        // (hack) we make the number of pixels in a row be 4*N, and set the 
        // corresponding alignment of OpenGL to 4
        if (img.cols % 4 != 0) {
            cv::resize(img, img, cv::Size(img.cols - img.cols % 4, img.rows));
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        // make sure the img is stored continously in memory
        assert(img.isContinuous());
        // make sure the rows of img are tightly packed
        assert(int(img.step[0] / img.elemSize()) == img.cols);
        glActiveTexture(GL_TEXTURE0);
        shared_ptr<Texture> t(new Texture());
        glBindTexture(GL_TEXTURE_2D, t->handle);
        GLint tex_fmt = (img.channels() == 3) ? GL_BGR : GL_BGRA;
        glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA,
                img.cols,
                img.rows,
                0,
                tex_fmt,
                GL_UNSIGNED_BYTE,
                img.ptr());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(
                GL_TEXTURE_2D,
                GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        CHECK_GL_ERROR;
        textures.push_back(t);
        b = t->handle;
        if (b == 0) {
            fprintf(stderr, "cannot bind texture '%s'\n", image_fn.c_str()); 
        }
    }

	bind_cache[image_fn] = b; // we go on, even if texture isn't there -- better
                              // than crash
	return b;
}

void Context::load_missing_textures() {
	shared_ptr<Household::World> world = weak_world.lock();
	if (!world) return;

	while (1) {
		bool did_anything = false;
		for (auto i = world->klass_cache.begin();
                i != world->klass_cache.end(); ++i) {
			shared_ptr<ThingyClass> klass = i->second.lock();
			if (!klass) {
				world->klass_cache.erase(i);
				did_anything = true;
				break;
			}
			shared_ptr<ShapeDetailLevels> v = klass->shapedet_visual;
			if (v->load_later_on) {
				v->load_later_on = false;
				load_model(
                        v, v->load_later_fn, v->scale, v->load_later_transform);
			}
		}
		if (!did_anything) break;
	}
	//fprintf(stderr, "world now has %i classes\n", (int)world->classes.size());
	for (const weak_ptr<Thingy>& w : world->drawlist) {
		shared_ptr<Thingy> t = w.lock();
		if (!t) {
            continue;
        }
		if (!t->klass) {
            continue;
        }            
		const shared_ptr<MaterialNamespace>& mats =
                t->klass->shapedet_visual->materials;
		if (!mats) {
            continue;
        }
		for (auto const& pair: mats->name2mtl) {
			shared_ptr<Material> mat = pair.second;
			if (mat->texture_loaded) {
                continue;
            }
			if (mat->diffuse_texture_image_fn.empty()) {
                continue;
            }
			mat->texture = cached_bind_texture(mat->diffuse_texture_image_fn);
			mat->texture_loaded = true;
		}
	}

	for (auto i = allocated_vaos.begin(); i != allocated_vaos.end(); ) {
		if (i->unique()) {
			i = allocated_vaos.erase(i);
			continue;
		}
		++i;
	}

	for (auto i = allocated_buffers.begin(); i != allocated_buffers.end(); ) {
		if (i->unique()) {
			i = allocated_buffers.erase(i);
			continue;
		}
		++i;
	}
}

static void render_lines_overlay(const shared_ptr<Shape>& t) {
	float r = float(1/256.0) * ((t->lines_color >> 16) & 255);
	float g = float(1/256.0) * ((t->lines_color >>  8) & 255);
	float b = float(1/256.0) * ((t->lines_color >>  0) & 255);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, t->lines.data());
	glColor3f(r, g, b);
	glDrawArrays(GL_LINES, 0, t->lines.size()/3);
	if (!t->raw_vertexes.empty()) {
		glColor3f(1.0f, 0.0f, 0.0f);
		glPointSize(3.0f);
		glVertexPointer(3,
                        sizeof(btScalar)==sizeof(float) ? GL_FLOAT : GL_DOUBLE,
                        0,
                        t->raw_vertexes.data());
		glDrawArrays(GL_POINTS, 0, t->raw_vertexes.size()/3);
	}
	glDisableClientState(GL_VERTEX_ARRAY);
}

void Context::initGL() {
	if (initialized) {
        return;
    }
	initialized = true;

	program_tex = GLShaderProgram::load_program(
            "simple_texturing.vert.glsl",
            "",
            "simple_texturing.frag.glsl");
	program_tex->link();
    program_tex->bind_attribute_location("input_vertex", ATTR_N_VERTEX);
    program_tex->bind_attribute_location("input_normal", ATTR_N_NORMAL);
    program_tex->bind_attribute_location("input_texcoord", ATTR_N_TEXCOORD);
	loc_input_matrix_modelview_inverse_transpose =
            program_tex->get_uniform_location(
                    "input_matrix_modelview_inverse_transpose");
	loc_input_matrix_modelview =
            program_tex->get_uniform_location("input_matrix_modelview");
	loc_enable_texture = program_tex->get_uniform_location("enable_texture");
	loc_texture = program_tex->get_uniform_location("texture_id");
	loc_uni_color = program_tex->get_uniform_location("uni_color");
	loc_multiply_color = program_tex->get_uniform_location("multiply_color");
	// can be -1 if uniform is actually unused in glsl code

	program_displaytex = GLShaderProgram::load_program(
            "fullscreen_triangle.vert.glsl",
            "",
            "displaytex.frag.glsl");
	program_displaytex->link();

#ifdef USE_SSAO
	if (ssao_enable)
		ssao_enable = _hbao_init();
#endif
    CHECK_GL_ERROR;
}

ContextViewport::ContextViewport(
        const shared_ptr<Context>& cx,
        int W,
        int H,
        double near,
        double far,
        double hfov) : cx(cx), W(W), H(H), near(near), far(far), hfov(hfov) {
	side = std::max(W, H);
#ifdef USE_SSAO
	_depthlinear_init();
#endif

	fbuf_scene.reset(new Framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, fbuf_scene->handle);
#ifndef USE_SSAO
	tex_color.reset(new Texture());
	glBindTexture(GL_TEXTURE_2D, tex_color->handle);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, W, H);
	glBindTexture(GL_TEXTURE_2D, 0);
	tex_depthstencil.reset(new Texture());
	glBindTexture(GL_TEXTURE_2D, tex_depthstencil->handle);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, W, H);
	glBindTexture(GL_TEXTURE_2D, 0);
#endif
	glFramebufferTexture(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            tex_color->handle,
            0);
	glFramebufferTexture(
            GL_FRAMEBUFFER,
            GL_DEPTH_STENCIL_ATTACHMENT,
            tex_depthstencil->handle,
            0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ContextViewport::_render_single_object(
        const shared_ptr<Household::ShapeDetailLevels>& m,
        uint32_t options,
        int detail,
        const glm::mat4& at_pos) {
	const std::vector<shared_ptr<Shape>>& shapes = m->detail_levels[detail];

	int cnt = shapes.size();
	for (int c = 0; c < cnt; c++) {
		shared_ptr<Shape> t = shapes[c];
		if (!t->vao) {
			if (t->primitive_type != Shape::MESH &&
                    t->primitive_type != Shape::STATIC_MESH) {
				primitives_to_mesh(m, detail, c);
            }
			cx->_shape_to_vao(t);
		}

        glm::mat4 shape_pos = btTransform_to_mat4(t->origin);
		
        glm::mat4 obj_modelview = modelview * at_pos * shape_pos;
		cx->program_tex->set_uniform_value(
                cx->loc_input_matrix_modelview,
                obj_modelview);
		cx->program_tex->set_uniform_value(
                cx->loc_input_matrix_modelview_inverse_transpose,
                glm::transpose(glm::inverse(obj_modelview)));

		uint32_t color = 0;
		uint32_t multiply_color = 0xFFFFFF;
		bool use_texture = false;
		bool meta = options & VIEW_METACLASS;
		if (!meta && t->material) {
			color = t->material->diffuse_color;
			multiply_color = t->material->multiply_color;
			use_texture = !t->t.empty() && t->material->texture;
		}
		if (options & VIEW_COLLISION_SHAPE) {
            color ^= (0xFFFFFF & (uint32_t) (uintptr_t) t.get());
        }
		int triangles = t->v.size();

		{
			float r = float(1/256.0) * ((multiply_color >> 16) & 255);
			float g = float(1/256.0) * ((multiply_color >>  8) & 255);
			float b = float(1/256.0) * ((multiply_color >>  0) & 255);
            cx->program_tex->set_uniform_value(
                    cx->loc_multiply_color,
                    glm::vec4(r, g, b, 1));
		}
		{
			float a = float(1/256.0) * ((color >> 24) & 255);
			if (a==0) a = 1; // allow to specify colors in simple form 0xAABBCC
			float r = float(1/256.0) * ((color >> 16) & 255);
			float g = float(1/256.0) * ((color >>  8) & 255);
			float b = float(1/256.0) * ((color >>  0) & 255);
			cx->program_tex->set_uniform_value(
                    cx->loc_uni_color,
                    glm::vec4(r, g, b, cx->pure_color_opacity*a));
		}

		glBindVertexArray(t->vao->handle);
		if (use_texture) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, t->material->texture);
		}
		cx->program_tex->set_uniform_value(
                cx->loc_enable_texture, use_texture);
		cx->program_tex->set_uniform_value(cx->loc_texture, 0);

		glDrawArrays(GL_TRIANGLES, 0, triangles/3);
        CHECK_GL_ERROR;
		glBindVertexArray(0);
	}
}

int ContextViewport::_objects_loop(int floor_visible, uint32_t view_options) {
	shared_ptr<Household::World> world = cx->weak_world.lock();
	if (!world) return 0;

	int ms_render_objectcount = 0;
	for (auto i=world->drawlist.begin(); i!=world->drawlist.end(); ) {
		shared_ptr<Thingy> t = i->lock();
		if (!t) {
			i = world->drawlist.erase(i);
			continue;
		}
		if (t->visibility_123 > floor_visible) {
			++i;
			continue;
		}

		ms_render_objectcount++;

        glm::mat4 pos = btTransform_to_mat4(t->bullet_position);
        glm::mat4 loc =
                btTransform_to_mat4(t->bullet_local_inertial_frame.inverse());

		_render_single_object(
                t->klass->shapedet_visual, view_options, DETAIL_BEST, pos*loc);

		++i;
	}

	return ms_render_objectcount;
}

void Context::_generate_ruler_vao()
{
	ruler_vao.reset(new VAO);
	glBindVertexArray(ruler_vao->handle);
	ruler_vertexes.reset(new Buffer);
	glBindBuffer(GL_ARRAY_BUFFER, ruler_vertexes->handle);
	glBufferData(
            GL_ARRAY_BUFFER, sizeof(line_vertex), line_vertex, GL_STATIC_DRAW);
	glVertexAttribPointer(ATTR_N_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(ATTR_N_VERTEX);
	glBindVertexArray(0);
    CHECK_GL_ERROR;
}

void Context::_shape_to_vao(const shared_ptr<Household::Shape>& shape)
{
	shape->vao.reset(new VAO);
	allocated_vaos.push_back(shape->vao);
	glBindVertexArray(shape->vao->handle);

	assert(shape->v.size() > 0);
	shape->buf_v.reset(new Buffer);
	allocated_buffers.push_back(shape->buf_v);
	glBindBuffer(GL_ARRAY_BUFFER, shape->buf_v->handle);
	glBufferData(
            GL_ARRAY_BUFFER,
            shape->v.size() * sizeof(shape->v[0]),
            shape->v.data(),
            GL_STATIC_DRAW);
	glVertexAttribPointer(ATTR_N_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	assert(shape->norm.size() > 0);
	shape->buf_n.reset(new Buffer);
	allocated_buffers.push_back(shape->buf_n);
	glBindBuffer(GL_ARRAY_BUFFER, shape->buf_n->handle);
	glBufferData(
            GL_ARRAY_BUFFER,
            shape->norm.size() * sizeof(shape->norm[0]),
            shape->norm.data(),
            GL_STATIC_DRAW);
	glVertexAttribPointer(ATTR_N_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	if (shape->t.size()) {
		shape->buf_t.reset(new Buffer);
		allocated_buffers.push_back(shape->buf_t);
		glBindBuffer(GL_ARRAY_BUFFER, shape->buf_t->handle);
		glBufferData(
                GL_ARRAY_BUFFER,
                shape->t.size()*sizeof(shape->t[0]),
                shape->t.data(),
                GL_STATIC_DRAW);
		glVertexAttribPointer(ATTR_N_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(ATTR_N_TEXCOORD);
	}

	glEnableVertexAttribArray(ATTR_N_VERTEX);
	glEnableVertexAttribArray(ATTR_N_NORMAL);
	glBindVertexArray(0);
    CHECK_GL_ERROR;
}

void ContextViewport::paint(
        float user_x,
        float user_y,
        float user_z,
        float wheel,
        float zrot,
        float xrot,
        Household::Camera* camera,
        int floor_visible,
        uint32_t view_options,
        float ruler_size) {
	if (!cx->program_tex) {
		cx->initGL();
		cx->_generate_ruler_vao();
	}

    // disable visibility_123 for robot cameras
	if (camera) floor_visible = 65535;
#ifdef USE_SSAO
	glBindFramebuffer(GL_FRAMEBUFFER, fbuf_scene->handle);
#else
	// For rendering without shadows, just keep rendering on default framebuffer
	if (camera) {
        // unless we render offscreen for camera
		glBindFramebuffer(GL_FRAMEBUFFER, fbuf_scene->handle);
    }
#endif
	glViewport(0,0,W,H);

	float clear_color[4] = { 0.8, 0.8, 0.9, 1.0 };
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClearBufferfv(GL_COLOR, 0, clear_color);
	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);

	double xmin, xmax;
	xmax = near * tanf(hfov * M_PI / 180 * 0.5);
	xmin = -xmax;
    glm::mat4 projection =
            glm::frustum(xmin, xmax, xmin*H/W, xmax*H/W, near, far);

    glm::mat4 matrix_view;
	if (!camera) {
		matrix_view = glm::translate(matrix_view, glm::vec3(0, 0, -0.5*wheel));
		matrix_view = glm::rotate(matrix_view, xrot, glm::vec3(1.0, 0.0, 0.0));
		matrix_view = glm::rotate(matrix_view, zrot, glm::vec3(0.0, 0.0, 1.0));
		matrix_view = glm::translate(
                matrix_view, glm::vec3( -user_x, -user_y, -user_z));
		//gl_Position = projMat * viewMat * modelMat * vec4(position, 1.0);
	} else {
		shared_ptr<Thingy> attached = camera->camera_attached_to.lock();
		btTransform t = attached ? attached->bullet_position.inverse() : camera->camera_pose.inverse();
        matrix_view = btTransform_to_mat4(t);
	}

	modelview = projection * matrix_view;
	modelview_inverse_transpose = glm::transpose(glm::inverse(modelview));

	if (cx->need_load_missing_textures) {
		cx->need_load_missing_textures = false;
		cx->load_missing_textures();
	}

	cx->program_tex->use();
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	cx->program_tex->set_uniform_value(cx->loc_enable_texture, false);
	cx->program_tex->set_uniform_value(cx->loc_uni_color, glm::vec4(0,0,0,0.8));
	cx->program_tex->set_uniform_value(cx->loc_texture, 0);
	cx->program_tex->set_uniform_value(
            cx->loc_input_matrix_modelview, modelview);
	cx->program_tex->set_uniform_value(
            cx->loc_input_matrix_modelview_inverse_transpose,
            modelview_inverse_transpose);
	if (~view_options & VIEW_CAMERA_BIT) {
		glBindVertexArray(cx->ruler_vao->handle);
        // error often here, when context different, also set if (1) above to 
        // always enter this code block
		CHECK_GL_ERROR;
        glDrawArrays(GL_LINES, 0, sizeof(line_vertex)/sizeof(float)/3);
		glBindVertexArray(0);
	}
	visible_object_count = _objects_loop(floor_visible, view_options);
	cx->program_tex->release();

#ifdef USE_SSAO
	if (cx->ssao_enable) {
		_hbao_prepare(&projection[0][0]);
		_depthlinear_paint(0);
		_ssao_run(0);
	}

	if (camera) {
        // buffer stays bound so camera can read pixels
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fbuf_scene->handle);
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, fbuf_scene->handle);
#endif
}

void ContextViewport::_texture_paint(GLuint h) {
	// FIXME
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	cx->program_displaytex->use();
	glBindTexture(GL_TEXTURE_2D, h);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void opengl_init(const shared_ptr<Household::World>& wref, int h, int w) {
	wref->cx.reset(new SimpleRender::Context(wref));
    wref->cx->glcx = createHeadlessContext(h, w);
    wref->cx->glcx->print_info();
    wref->cx->ssao_enable = true;
}

} // namespace
