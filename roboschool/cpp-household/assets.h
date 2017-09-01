#pragma once
#include <BulletDynamics/btBulletDynamicsCommon.h>
#include <set>
#include <map>
#include <list>
#include <vector>

#include "common.h"

namespace SimpleRender {
struct VAO;
struct Buffer;
struct Context;
class ContextViewport;
}

struct App;

namespace Household {

struct Material {
	Material(const std::string& name): name(name)  { }
	std::string name;
	uint32_t texture = 0;
	bool texture_loaded = false;
	std::string diffuse_texture_image_fn;
	uint32_t diffuse_color  = 0x00FF00;
	uint32_t multiply_color = 0xFFFFFF;
};

struct Cylinder {
	btScalar radius;
	btScalar length;
};

struct Sphere {
	btScalar radius;
};

struct Box {
	btScalar size_x;
	btScalar size_y;
	btScalar size_z;
};

struct Shape {
	btTransform origin;
	enum { MESH, STATIC_MESH, CYLINDER, SPHERE, CAPSULE, BOX, DEBUG_LINES };
	int primitive_type = MESH;

	smart_pointer::shared_ptr<Cylinder> cylinder;
	smart_pointer::shared_ptr<Sphere> sphere;
	smart_pointer::shared_ptr<Box> box;

	bool converted_to_mesh = false;
	void convert_primitive_to_mesh();

	std::vector<btScalar> raw_vertexes; // minimal number of vertices, intended for convex hull

	std::vector<float> v;  // v, t, norm, color -- all prepared for rendering
	std::vector<float> t;
	std::vector<float> norm;
	std::vector<float> lines; // same facets walked around to draw lines
	uint32_t lines_color = 0xFFFFFF;
	void push_vertex(btScalar vx, btScalar vy, btScalar vz); // utilities for mesh generation
	void push_normal(btScalar nx, btScalar ny, btScalar nz);
	void push_tex(btScalar u, btScalar v);
	void push_lines(btScalar x, btScalar y, btScalar z);

	smart_pointer::shared_ptr<Material> material;
	smart_pointer::shared_ptr<SimpleRender::VAO> vao;
	smart_pointer::shared_ptr<SimpleRender::Buffer> buf_v;
	smart_pointer::shared_ptr<SimpleRender::Buffer> buf_n;
	smart_pointer::shared_ptr<SimpleRender::Buffer> buf_t;
	smart_pointer::shared_ptr<SimpleRender::Buffer> buf_l;

	Shape()  { origin.setIdentity(); }
};

const int DETAIL_LEVELS = 2;
const int DETAIL_BEST   = 0;
const int DETAIL_LOWER  = 1;

struct MaterialNamespace {
	std::map<std::string, smart_pointer::shared_ptr<Material>> name2mtl;
};

struct ShapeDetailLevels {
	bool load_later_on = false;
	std::string load_later_fn;
	btTransform load_later_transform;
	smart_pointer::shared_ptr<MaterialNamespace> materials;
	std::vector<smart_pointer::shared_ptr<Shape>> detail_levels[DETAIL_LEVELS];
};

void load_model(const smart_pointer::shared_ptr<ShapeDetailLevels>& result, const std::string& fn, btScalar scale, const btTransform& transform);
bool load_collision_shape_from_OFF_files(const smart_pointer::shared_ptr<ShapeDetailLevels>& result, const std::string& fn_template, btScalar scale, const btTransform& viz_frame);

} // namespace Household
