// Copyright (c) 2017 Baidu Inc. All Rights Reserved.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <QtWidgets/QApplication>
#include "render-glwidget.h"
#include "roboschool_API.h"

using smart_pointer::shared_ptr;
using smart_pointer::weak_ptr;
using smart_pointer::make_shared;

extern std::string glsl_path;

namespace Household {
    btScalar SCALE = 1.0;
    btScalar COLLISION_MARGIN = 0.002*SCALE;
}

struct App {
    static shared_ptr<App> instance(const shared_ptr<Household::World>& wref) {
        static shared_ptr<App> instance(make_shared<App>(wref));
        wref->app_ref = instance;

        return instance;
    }

    virtual ~App() {
        delete app_;
    }

    App(const shared_ptr<Household::World>& wref) {
        SimpleRender::opengl_init_before_app(wref);
        static int argc = 1;
        static const char* argv[] = { "Roboschool Simulator" };
        QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);
        app_ = new QApplication(argc, const_cast<char**>(argv));
        SimpleRender::opengl_init(wref->cx);
    }

    QApplication* app_;
};

namespace roboschool {

const double SCALE = Household::SCALE;
const double COLLISION_MARGIN = Household::COLLISION_MARGIN;

/************************************ Pose ************************************/
class PoseImpl {
public:
    static Pose from_bt_transform(const btTransform& tr);

    static btTransform to_bt_transform(const Pose& p);

    static void rotate_z(Pose& p, double angle);

    static Pose dot(const Pose& p1, const Pose& p2);
};

Pose PoseImpl::from_bt_transform(const btTransform& tr) {
    btVector3 t = tr.getOrigin();
    btQuaternion q = tr.getRotation();
    Pose p;
    p.set_xyz(double(t.x() / SCALE),
              double(t.y() / SCALE),
              double(t.z() / SCALE));
    p.set_quaternion(double(q.x()),
                     double(q.y()),
                     double(q.z()),
                     double(q.w()));
    return p;
}

btTransform PoseImpl::to_bt_transform(const Pose& p) {
    return btTransform(btQuaternion(p.qx_, p.qy_, p.qz_, p.qw_),
                       btVector3(p.x_, p.y_, p.z_));
}

void PoseImpl::rotate_z(Pose& p, double angle) {
    btQuaternion t(p.qx_, p.qy_, p.qz_, p.qw_);
    btQuaternion t2;
    t2.setRotation(btVector3(0,0,1), angle);
    t = t2 * t;
    p.set_quaternion(t.x(), t.y(), t.z(), t.w());
}

Pose PoseImpl::dot(const Pose& p1, const Pose& p2) {
    return Pose(from_bt_transform(to_bt_transform(p1) *
                                  to_bt_transform(p2)));
}

std::tuple<double,double,double> Pose::xyz() {
    return std::make_tuple(x_ / SCALE, y_ / SCALE, z_ / SCALE);
}

void Pose::set_xyz(double x, double y, double z) {
    x_ = x * SCALE;
    y_ = y * SCALE;
    z_ = z * SCALE;
}

void Pose::move_xyz(double x, double y, double z) {
    x_ += x * SCALE;
    y_ += y * SCALE;
    z_ += z * SCALE;
}

std::tuple<double,double,double> Pose::rpy() const {
    double sqw = qw_ * qw_;
    double sqx = qx_ * qx_;
    double sqy = qy_ * qy_;
    double sqz = qz_ * qz_;
    double t2 = -2.0 * (qx_ * qz_ - qy_ * qw_) / (sqx + sqy + sqz + sqw);
    double yaw   = atan2(2.0 * (qx_ * qy_ + qz_ * qw_), ( sqx - sqy - sqz + sqw));
    double roll  = atan2(2.0 * (qy_ * qz_ + qx_ * qw_), (-sqx - sqy + sqz + sqw));
    t2 = t2 >  1.0f ?  1.0f : t2;
    t2 = t2 < -1.0f ? -1.0f : t2;
    double pitch = asin(t2);
    return std::make_tuple(roll, pitch, yaw);
}

void Pose::set_rpy(double r, double p, double y) {
    double t0 = cos(y * 0.5);
    double t1 = sin(y * 0.5);
    double t2 = cos(r * 0.5);
    double t3 = sin(r * 0.5);
    double t4 = cos(p * 0.5);
    double t5 = sin(p * 0.5);
    qw_ = t0 * t2 * t4 + t1 * t3 * t5;
    qx_ = t0 * t3 * t4 - t1 * t2 * t5;
    qy_ = t0 * t2 * t5 + t1 * t3 * t4;
    qz_ = t1 * t2 * t4 - t0 * t3 * t5;
}

void Pose::rotate_z(double angle) {
    PoseImpl::rotate_z(*this, angle);
}

Pose Pose::dot(const Pose& other) {
    return PoseImpl::dot(*this, other);
}

/*********************************** Thingy ***********************************/
class ThingyImpl {
public:
    ThingyImpl(const shared_ptr<Household::Thingy>& t,
               const weak_ptr<Household::World>& w)
            : tref(t), wref(w) {}

    Pose pose() const {
        return PoseImpl::from_bt_transform(tref->bullet_position);
    }

    std::tuple<double,double,double> speed() {
        assert(tref->bullet_queried_at_least_once);
        return std::make_tuple(tref->bullet_speed.x() / SCALE,
                               tref->bullet_speed.y() / SCALE,
                               tref->bullet_speed.z() / SCALE);
    }

    std::tuple<double,double,double> angular_speed() {
        assert(tref->bullet_queried_at_least_once);
        return std::make_tuple(tref->bullet_angular_speed.x(),
                               tref->bullet_angular_speed.y(),
                               tref->bullet_angular_speed.z());
    }

    void set_name(const std::string& name) {
        tref->name = name;
    }

    std::string get_name() {
        return tref->name;
    }

    void set_visibility_123(int f)  {
        tref->visibility_123 = f;
    }

    int get_visibility_123()  {
        return tref->visibility_123;
    }

    void set_multiply_color(const std::string& tex, uint32_t c) {
        // this works on mostly white textures
        tref->set_multiply_color(tex, &c, 0);
    }

    void assign_metaclass(uint8_t mclass) {
        tref->klass->metaclass = mclass;
    }

    std::vector<shared_ptr<ThingyImpl>> contact_list();

private:
    shared_ptr<Household::Thingy> tref;
    weak_ptr<Household::World> wref;
    std::vector<weak_ptr<Household::Thingy>> sleep_list;

};

std::vector<shared_ptr<ThingyImpl>> ThingyImpl::contact_list() {
    std::vector<shared_ptr<ThingyImpl>> r;
    if (auto world = wref.lock()) {
        if (!tref->is_sleeping()) {
            sleep_list.clear();
            for (const auto &t: world->bullet_contact_list(tref)) {
                r.push_back(make_shared<ThingyImpl>(t, wref));
                sleep_list.push_back(t);
            }
        } else {
            for (const auto& o: sleep_list) {
                if (auto t = o.lock()) {
                    r.push_back(make_shared<ThingyImpl>(t, wref));
                }
            }
        }
    }

    return r;
}

inline std::tuple<double,double,double> Thingy::speed() {
    return impl_->speed();
}

inline std::tuple<double,double,double> Thingy::angular_speed() {
    return impl_->angular_speed();
}

inline void Thingy::set_name(const std::string& name) { impl_->set_name(name); }

inline std::string Thingy::get_name() { return impl_->get_name(); }

inline void Thingy::set_visibility_123(int f) {
    impl_->set_visibility_123(f);
}

inline int Thingy::get_visibility_123() { return impl_->get_visibility_123(); }

inline void Thingy::set_multiply_color(const std::string& tex, uint32_t c) {
    impl_->set_multiply_color(tex, c);
}

inline void Thingy::assign_metaclass(uint8_t mclass) {
    impl_->assign_metaclass(mclass);
}

Pose Thingy::pose() const { return impl_->pose(); }

std::vector<Thingy> Thingy::contact_list() {
    std::vector<shared_ptr<ThingyImpl>> tlist = impl_->contact_list();
    std::vector<Thingy> ret;
    for (auto& t : tlist) {
        ret.emplace_back(t);
    }
    return ret;
}

/*********************************** Joint ***********************************/
struct Joint {
    shared_ptr<Household::Joint> jref;

    Joint(const shared_ptr<Household::Joint>& j) : jref(j) {}

    std::string name() {
        return jref->joint_name;
    }

    void set_motor_torque(double q) {
        jref->set_motor_torque(q);
    }

    void set_target_speed(double target_speed, double kd, double maxforce) {
        jref->set_target_speed(target_speed, kd, maxforce);
    }

    void set_servo_target(double target_pos, double kp, double kd, double maxforce) {
        jref->set_servo_target(target_pos, kp, kd, maxforce);
    }

    void reset_current_position(double pos, double vel) {
        jref->reset_current_position(pos, vel);
    }

    std::tuple<double,double> current_position() {
        return std::make_tuple(jref->joint_current_position,
                               jref->joint_current_speed);
    }
    std::tuple<double,double> current_relative_position() {
        float pos;
        float speed;
        jref->joint_current_relative_position(&pos, &speed);
        return std::make_tuple(pos, speed);
    }

    std::tuple<double,double,double,double> limits() {
        return std::make_tuple(jref->joint_limit1,
                               jref->joint_limit2,
                               jref->joint_max_force,
                               jref->joint_max_velocity);
    }

    std::string type() {
        switch (jref->joint_type) {
            case Household::Joint::ROTATIONAL_MOTOR:
                return "motor";
                break;
            case Household::Joint::LINEAR_MOTOR:
                return "linear_motor";
                break;
            default:
                return "unknown";
        }
    }
};

/*********************************** Object ***********************************/
class ObjectImpl {
    friend class WorldImpl;
public:
    ObjectImpl(const shared_ptr<Household::Robot>& r,
               const weak_ptr<Household::World>& w)
            : rref(r), wref(w) {
    }

    void destroy();

    Thingy root_part() {
        return Thingy(make_shared<ThingyImpl>(rref->root_part, wref));
    }

    Pose pose() const {
        return PoseImpl::from_bt_transform(rref->root_part->bullet_position);
    }

    void set_pose(const Pose& p) {
        if (auto world = wref.lock()) {
            world->robot_move(
                    rref, PoseImpl::to_bt_transform(p), btVector3(0,0,0));
        }
    }

    void speed(double& vx, double& vy, double& vz) const {
        vx = rref->root_part->bullet_speed[0] / SCALE;
        vy = rref->root_part->bullet_speed[1] / SCALE;
        vz = rref->root_part->bullet_speed[2] / SCALE;
    }

    double speed_x() const {
        return (rref->root_part->bullet_speed[0] / SCALE);
    }

    double speed_y() const {
        return (rref->root_part->bullet_speed[1] / SCALE);
    }

    double speed_z() const {
        return (rref->root_part->bullet_speed[2] / SCALE);
    }

    void set_speed(double vx, double vy, double vz) {
        if (auto world = wref.lock()) {
            world->robot_move(
                    rref, rref->root_part->bullet_position, btVector3(vx,vy,vz));
        }
    }

    void set_pose_and_speed(const Pose& p,
                            double vx, double vy, double vz) {
        if (auto world = wref.lock()) {
            world->robot_move(
                    rref, PoseImpl::to_bt_transform(p), btVector3(vx,vy,vz));
        }
    }

    void query_position() {
        // necessary for robot that is just created, before any step() done
        if (auto world = wref.lock()) {
            world->query_body_position(rref);
        }
    }

private:
    shared_ptr<Household::Robot> rref;
    weak_ptr<Household::World> wref;
};

void ObjectImpl::destroy() {
    rref->bullet_handle = -1;
    for (auto& p : rref->robot_parts) {
        assert(p.use_count() == 1);
        p.reset();
    }
    for (auto& j : rref->joints) {
        assert(j.use_count() == 1);
        j.reset();
    }
    for (auto& c : rref->cameras) {
        assert(c.use_count() == 1);
        c.reset();
    }
}

void Object::destroy() {
    impl_->destroy();
}

Thingy Object::root_part() {
    return impl_->root_part();
}

Pose Object::pose() const {
    return impl_->pose();
}

void Object::set_pose(const Pose& p) {
    impl_->set_pose(p);
}

void Object::speed(double& vx, double& vy, double& vz) const {
    impl_->speed(vx, vy, vz);
}

double Object::speed_x() const {
    return impl_->speed_x();
}

double Object::speed_y() const {
    return impl_->speed_y();
}

double Object::speed_z() const {
    return impl_->speed_z();
}

void Object::set_speed(double vx, double vy, double vz) {
    impl_->set_speed(vx, vy, vz);
}

void Object::set_pose_and_speed(
        const Pose& p, double vx, double vy, double vz) {
    impl_->set_pose_and_speed(p, vx, vy, vz);
}

void Object::query_position() {
    impl_->query_position();
}

/*********************************** Camera ***********************************/
class CameraImpl {
public:
    CameraImpl(const shared_ptr<Household::Camera>& cref,
               const weak_ptr<Household::World>& wref) :
            cref(cref), wref(wref), app(nullptr) {}

    ~CameraImpl() {
        app.reset();
    }

    std::string name() {
        return cref->camera_name;
    }

    std::tuple<int,int> resolution() {
        return std::make_tuple(cref->camera_res_w, cref->camera_res_h);
    }

    Pose pose() {
        return PoseImpl::from_bt_transform(cref->camera_pose);
    }

    void set_pose(const Pose& p) {
        cref->camera_pose = PoseImpl::to_bt_transform(p);
    }

    void set_hfov(double hor_fov) {
        cref->camera_hfov = hor_fov;
    }

    void set_near(double near) {
        cref->camera_near = near;
    }

    void set_far(double far) {
        cref->camera_near = far;
    }

    RenderResult render(bool render_depth,
                        bool render_labeling,
                        bool print_timing);

    void move_and_look_at(double from_x, double from_y, double from_z,
                          double obj_x, double obj_y, double obj_z);

private:
    shared_ptr<Household::Camera> cref;
    weak_ptr<Household::World> wref;
    shared_ptr<App> app;
};

RenderResult CameraImpl::render(bool render_depth,
                                bool render_labeling,
                                bool print_timing) {
    auto world = wref.lock();
    assert(world);
    if (!app) {
        app = App::instance(world);
    }

    cref->camera_render(
            world->cx, render_depth, render_labeling, print_timing);

    return std::make_tuple(
            cref->camera_rgb,
            render_depth ? cref->camera_depth_mask : std::string(),
            render_labeling ? cref->camera_labeling : std::string(),
            render_labeling ? cref->camera_labeling_mask : std::string(),
            cref->camera_res_h, cref->camera_res_w);
}

void CameraImpl::move_and_look_at(double from_x, double from_y, double from_z,
                                  double obj_x, double obj_y, double obj_z) {
    Pose pose;
    double dist = sqrt( square(obj_x-from_x) + square(obj_y-from_y) );
    pose.set_rpy(M_PI/2 + atan2(obj_z-from_z, dist), 0, 0);
    pose.rotate_z( atan2(obj_y-from_y, obj_x-from_x) - M_PI/2 );
    pose.move_xyz( from_x, from_y, from_z );
    set_pose(pose);
}

std::string Camera::name() {
    return impl_->name();
}

std::tuple<int,int> Camera::resolution() {
    return impl_->resolution();
}

Pose Camera::pose() {
    return impl_->pose();
}

void Camera::set_pose(const Pose& p) {
    impl_->set_pose(p);
}

void Camera::set_hfov(double hor_fov) {
    impl_->set_hfov(hor_fov);
}

void Camera::set_near(double near) {
    impl_->set_near(near);
}

void Camera::set_far(double far) {
    impl_->set_far(far);
}

RenderResult Camera::render(bool render_depth,
                            bool render_labeling,
                            bool print_timing) {
    return impl_->render(render_depth, render_labeling, print_timing);
}

void Camera::move_and_look_at(
        double from_x, double from_y, double from_z,
        double obj_x, double obj_y, double obj_z) {
    impl_->move_and_look_at(from_x, from_y, from_z,
                            obj_x, obj_y, obj_z);
}

/*********************************** World ************************************/
class WorldImpl {
public:
    WorldImpl(double gravity, double timestep) {
        wref.reset(new Household::World);
        wref->bullet_init(gravity * SCALE, timestep);
    }

    ~WorldImpl() {
        printf("WorldImpl destructor\n");
    }

    void remove_object(const Object& o);

    void remove_object(const ObjectImpl& obj_impl);

    void remove_object(const shared_ptr<ObjectImpl>& obj_impl) {
        remove_object(*(obj_impl));
    }

    void clean_everything() {
        wref->clean_everything();
    }

    Thingy load_thingy(const std::string& fn,
                       const Pose& p,
                       double scale,
                       double mass,
                       int color,
                       bool decoration_only) {

        return Thingy(make_shared<ThingyImpl>(
                wref->load_thingy(fn, PoseImpl::to_bt_transform(p),
                                  scale * SCALE, mass, color, decoration_only),
                wref));
    }

    Object load_urdf(const std::string& fn,
                     const Pose& p,
                     bool fixed_base,
                     bool self_collision) {
        return Object(make_shared<ObjectImpl>(
                wref->load_urdf(fn, PoseImpl::to_bt_transform(p),
                                fixed_base, self_collision),
                wref));
    }

    std::vector<Object> load_mjcf(const std::string& fn) {
        std::list<shared_ptr<Household::Robot>> rlist =
                wref->load_sdf_mjcf(fn, true);
        std::vector<Object> ret;
        for (auto r: rlist)
            ret.emplace_back(make_shared<ObjectImpl>(r, wref));

        return ret;
    }

    double ts() {
        return wref->ts;
    }

    bool step(int repeat) {
        wref->bullet_step(repeat);

        return false;
    }

    Camera new_camera_free_float(int camera_res_w, int camera_res_h,
                                 const std::string& camera_name);

    void set_glsl_path(const std::string& dir);

private:
    shared_ptr<Household::World> wref;
};

void WorldImpl::remove_object(const Object& o) {
    for (auto it = wref->robotlist.begin(); it != wref->robotlist.end(); /**/) {
        auto r = it->lock();
        if (r && r->bullet_handle == o.impl()->rref->bullet_handle) {
            it = wref->robotlist.erase(it);
        } else {
            it++;
        }
    }
    for (auto it = wref->drawlist.begin(); it != wref->drawlist.end(); /**/) {
        auto r = it->lock();
        if (r && r->bullet_handle == o.impl()->rref->bullet_handle) {
            it = wref->drawlist.erase(it);
        } else {
            it++;
        }
    }
}

Camera WorldImpl::new_camera_free_float(int camera_res_w, int camera_res_h,
                                        const std::string& camera_name) {
    shared_ptr<Household::Camera> cam(new Household::Camera);
    cam->camera_name = camera_name;
    cam->camera_res_w = camera_res_w;
    cam->camera_res_h = camera_res_h;
    return Camera(make_shared<CameraImpl>(cam, wref));
}

void WorldImpl::set_glsl_path(const std::string& dir) {
    glsl_path = dir;
}

World::World(double gravity, double timestep) :
        impl_(make_shared<WorldImpl>(gravity, timestep)) {}

void World::remove_object(const Object& obj) {
    impl_->remove_object(obj);
}

void World::clean_everything() {
    impl_->clean_everything();
}

Thingy World::load_thingy(const std::string& fn,
                          const Pose& pose,
                          double scale,
                          double mass,
                          int color,
                          bool decoration_only) {

    return impl_->load_thingy(fn, pose, scale, mass, color, decoration_only);
}

Object World::load_urdf(const std::string& fn,
                        const Pose& pose,
                        bool fixed_base,
                        bool self_collision) {
    return impl_->load_urdf(fn, pose, fixed_base, self_collision);
}

std::vector<Object> World::load_mjcf(const std::string& fn) {
    return impl_->load_mjcf(fn);
}

double World::ts() {
    return impl_->ts();
}

bool World::step(int repeat) {
    return impl_->step(repeat);
}

Camera World::new_camera_free_float(int camera_res_w, int camera_res_h,
                                             const std::string& camera_name) {
    return Camera(impl_->new_camera_free_float(
            camera_res_w, camera_res_h, camera_name));
}

void World::set_glsl_path(const std::string& dir) {
    impl_->set_glsl_path(dir);
}

} // roboschool
