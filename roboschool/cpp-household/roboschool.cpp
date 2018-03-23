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

#include "render-simple.h"
#include "roboschool_API.h"

using std::shared_ptr;
using std::weak_ptr;
using std::make_shared;

namespace roboschool {

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
    p.set_xyz(double(t.x()),
              double(t.y()),
              double(t.z()));
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
    return std::make_tuple(x_, y_, z_);
}

void Pose::set_xyz(double x, double y, double z) {
    x_ = x;
    y_ = y;
    z_ = z;
}

void Pose::move_xyz(double x, double y, double z) {
    x_ += x;
    y_ += y;
    z_ += z;
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
        return std::make_tuple(tref->bullet_speed.x(),
                               tref->bullet_speed.y(),
                               tref->bullet_speed.z());
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

    int bullet_handle() const { return tref->bullet_handle; }

    std::list<shared_ptr<ThingyImpl>> contact_list();

private:
    shared_ptr<Household::Thingy> tref;
    weak_ptr<Household::World> wref;
    std::vector<weak_ptr<Household::Thingy>> sleep_list;

};

std::list<shared_ptr<ThingyImpl>> ThingyImpl::contact_list() {
    std::list<shared_ptr<ThingyImpl>> r;
    auto world = wref.lock();
    if (world && tref->bullet_handle >= 0) {
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

int Thingy::bullet_handle() const { return impl_->bullet_handle(); }

std::list<Thingy> Thingy::contact_list() {
    std::list<shared_ptr<ThingyImpl>> tlist = impl_->contact_list();
    std::list<Thingy> ret;
    for (auto& t : tlist) {
        ret.emplace_back(t);
    }
    return ret;
}

/*********************************** Joint ***********************************/
class Joint {
    shared_ptr<Household::Joint> jref;

    Joint(const shared_ptr<Household::Joint>& j) : jref(j) {}

    std::string name() {
        return jref->joint_name;
    }

    void set_motor_torque(double q) {
        jref->set_motor_torque(q);
    }

    void set_target_speed(double target_speed, double maxforce) {
        jref->set_target_speed(target_speed, maxforce);
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
        jref->current_relative_position(&pos, &speed);
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

// std::string Joint::name() {
//     return impl_->name();
// }

// void Joint::set_servo_target(double target_pos, double kp, double kd, double maxforce) {
//     impl_->set_servo_target(target_pos, kp, kd, maxforce);
// }

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

    Pose part_pose(const int part_id) const {
        return PoseImpl::from_bt_transform(rref->part_pose(part_id));
    }

    void set_pose(const Pose& p) {
        if (auto world = wref.lock()) {
            world->robot_move(
                    rref, PoseImpl::to_bt_transform(p), btVector3(0,0,0));
        }
    }

    void speed(double& vx, double& vy, double& vz) const {
        vx = rref->root_part->bullet_speed[0];
        vy = rref->root_part->bullet_speed[1];
        vz = rref->root_part->bullet_speed[2];
    }

    double speed_x() const {
        return (rref->root_part->bullet_speed[0]);
    }

    double speed_y() const {
        return (rref->root_part->bullet_speed[1]);
    }

    double speed_z() const {
        return (rref->root_part->bullet_speed[2]);
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

    int bullet_handle() const { return rref->bullet_handle; }

    std::list<shared_ptr<ThingyImpl>> contact_list();

    // joint controls
    void joint_set_target_speed(
            const size_t joint_id, const float target_speed) {
        assert(joint_id >= 0 && joint_id < rref->joints.size());
        rref->joints[joint_id]->set_target_speed(target_speed, 400);
    }

    void joint_set_servo_target(
            const size_t joint_id, const double target_pos) {
        assert(joint_id >= 0 && joint_id < rref->joints.size());
        rref->joints[joint_id]->set_servo_target(target_pos, 1, 1, 40);
    }

    void joint_set_relative_servo_target(
            const size_t joint_id, const double target_pos) {
        assert(joint_id >= 0 && joint_id < rref->joints.size());
        rref->joints[joint_id]->set_relative_servo_target(target_pos, 1, 1);
    }

    void joint_current_position(const size_t joint_id, float& pos, float& vel) {
        assert(joint_id >= 0 && joint_id < rref->joints.size());
        pos = rref->joints[joint_id]->joint_current_position;
        vel = rref->joints[joint_id]->joint_current_speed;
    }

    void joint_current_relative_position(
            const size_t joint_id, float& pos, float& vel) {
        assert(joint_id >= 0 && joint_id < rref->joints.size());
        rref->joints[joint_id]->current_relative_position(&pos, &vel);
    }

    void joint_reset_current_position(
            const size_t joint_id, const float pos, const float vel) {
        assert(joint_id >= 0 && joint_id < rref->joints.size());
        rref->joints[joint_id]->reset_current_position(pos, vel);
    }

private:
    shared_ptr<Household::Robot> rref;
    weak_ptr<Household::World> wref;
};

std::list<shared_ptr<ThingyImpl>> ObjectImpl::contact_list() {
    auto t = ThingyImpl(rref->root_part, wref);
    std::list<shared_ptr<ThingyImpl>> ret = t.contact_list();
    for (auto& p : rref->robot_parts) {
        auto t = ThingyImpl(p, wref);
        auto l = t.contact_list();
        ret.insert(ret.end(), l.begin(), l.end());
    }

    return ret;
}

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
    return this->part_pose(-1);
}

Pose Object::part_pose(const int part_id = -1) const {
    return impl_->part_pose(part_id);
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

int Object::bullet_handle() const {
    return impl_->bullet_handle();
}

std::list<Thingy> Object::contact_list() {
    std::list<shared_ptr<ThingyImpl>> tlist = impl_->contact_list();
    std::list<Thingy> ret;
    for (auto& t : tlist) {
        ret.emplace_back(t);
    }
    return ret;
}

// joint controls
void Object::joint_set_target_speed(
        const size_t joint_id, const float target_speed) {
    impl_->joint_set_target_speed(joint_id, target_speed);
}

void Object::joint_set_servo_target(
        const size_t joint_id, const double target_pos) {
    impl_->joint_set_servo_target(joint_id, target_pos);
}

void Object::joint_set_relative_servo_target(
        const size_t joint_id, const double target_pos) {
    impl_->joint_set_relative_servo_target(joint_id, target_pos);
}

void Object::joint_current_position(
        const size_t joint_id, float& pos, float& vel) {
    impl_->joint_current_position(joint_id, pos, vel);
}

void Object::joint_current_relative_position(
        const size_t joint_id, float& pos, float& vel) {
    impl_->joint_current_relative_position(joint_id, pos, vel);
}

void Object::joint_reset_current_position(
        const size_t joint_id, const float pos, const float vel) {
    impl_->joint_reset_current_position(joint_id, pos, vel);
}

/*********************************** Camera ***********************************/
class CameraImpl {
public:
    CameraImpl(const shared_ptr<Household::Camera>& cref,
               const weak_ptr<Household::World>& wref) :
            cref(cref), wref(wref) {}

    ~CameraImpl() {} 

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
};

RenderResult CameraImpl::render(bool render_depth,
                                bool render_labeling,
                                bool print_timing) {
    auto world = wref.lock();
    assert(world);

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
    WorldImpl(int img_height, int img_width, double gravity, double timestep) {
        wref.reset(new Household::World);
        wref->bullet_init(gravity, timestep);
        SimpleRender::opengl_init(wref, img_height, img_width);
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
                                  scale, mass, color, decoration_only),
                wref));
    }

    Object load_urdf(const std::string& fn,
                     const Pose& p,
                     float scale,
                     bool fixed_base,
                     bool self_collision,
                     bool use_multibody) {
        return Object(make_shared<ObjectImpl>(
                wref->load_urdf(fn, PoseImpl::to_bt_transform(p), scale,
                                fixed_base, self_collision, use_multibody),
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

World::World(int height, int width, double gravity, double timestep) :
        impl_(make_shared<WorldImpl>(height, width, gravity, timestep)) {}

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
                        float scale,
                        bool fixed_base,
                        bool self_collision,
                        bool use_multibody) {
    return impl_->load_urdf(
            fn, pose, scale, fixed_base, self_collision, use_multibody);
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
