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

#pragma once

#include <math.h>
#include <tuple>
#include <vector>

#include "common.h"

namespace roboschool {

inline double square(double x)  { return x*x; }

struct PoseImpl;
struct ThingyImpl;
struct ObjectImpl;
struct CameraImpl;
struct WorldImpl;

/************************************ Pose ************************************/
class Pose {
    friend struct PoseImpl;
    friend struct ThingyImpl;
    friend struct ObjectImpl;
    friend struct CameraImpl;
    friend struct WorldImpl;
public:
    Pose() : x_(0), y_(0), z_(0), qx_(0), qy_(0), qz_(0), qw_(1) {}

    Pose(double x, double y, double z) : x_(x), y_(y), z_(z),
                                         qx_(0), qy_(0), qz_(0), qw_(1) {}

    Pose(const Pose& pose) {
        x_ = pose.x_;
        y_ = pose.y_;
        z_ = pose.z_;
        qx_ = pose.qx_;
        qy_ = pose.qy_;
        qz_ = pose.qz_;
        qw_ = pose.qw_;
    }

    double x() const { return x_; };
    double y() const { return y_; };
    double z() const { return z_; };
    double qx() const { return qx_; };
    double qy() const { return qy_; };
    double qz() const { return qz_; };
    double qw() const { return qw_; };

    std::tuple<double,double,double> xyz();

    void set_xyz(double x, double y, double z);

    void move_xyz(double dx, double dy, double dz);

    std::tuple<double,double,double,double> quatertion();

    void set_quaternion(double x, double y, double z, double w);

    std::tuple<double,double,double> rpy() const;

    void set_rpy(double r, double p, double y);

    void rotate_z(double angle);

    Pose dot(const Pose& other);

private:
    double x_, y_, z_;
    double qx_, qy_, qz_, qw_;
};

inline std::tuple<double,double,double,double> Pose::quatertion() {
    return std::make_tuple(qx_, qy_, qz_, qw_);
}

inline void Pose::set_quaternion(double x, double y, double z, double w) {
    qx_ = x;
    qy_ = y;
    qz_ = z;
    qw_ = w;
}

/*********************************** Thingy ***********************************/
class Thingy {
public:
    Thingy() : impl_(nullptr) {}

    Thingy(const smart_pointer::shared_ptr<ThingyImpl>& impl) : impl_(impl) {}

    Pose pose() const;

    std::tuple<double,double,double> speed();

    std::tuple<double,double,double> angular_speed();

    void set_name(const std::string& name);

    std::string get_name();

    void set_visibility_123(int f);

    int get_visibility_123();

    void set_multiply_color(const std::string& tex, uint32_t c);

    void assign_metaclass(uint8_t mclass);

    std::vector<Thingy> contact_list();

private:
    smart_pointer::shared_ptr<ThingyImpl> impl_;
};

/*********************************** Object ***********************************/
class Object {
    friend class World;
public:
    Object(const smart_pointer::shared_ptr<ObjectImpl>& impl) : impl_(impl) {}

    Object() : impl_(nullptr) {}

    const smart_pointer::shared_ptr<ObjectImpl>& impl() const {
        return impl_;
    }

    smart_pointer::shared_ptr<ObjectImpl>& mutable_impl() {
        return impl_;
    }

    void destroy();

    Thingy root_part();

    Pose pose() const;

    void set_pose(const Pose& p);

    void speed(double& vx, double& vy, double& vz) const;

    double speed_x() const;

    double speed_y() const;

    double speed_z() const;

    void set_speed(double vx, double vy, double vz);

    void set_pose_and_speed(const Pose& p, double vx, double vy, double vz);

    void query_position();

private:
    smart_pointer::shared_ptr<ObjectImpl> impl_;
};

/*********************************** Camera ***********************************/
typedef std::tuple<std::string, std::string,
                   std::string, std::string,
                   int, int> RenderResult;

class Camera {
public:
    Camera(const smart_pointer::shared_ptr<CameraImpl>& impl) : impl_(impl) {}

    std::string name();

    std::tuple<int,int> resolution();

    Pose pose();

    void set_pose(const Pose& p);

    void set_hfov(double hor_fov);

    void set_near(double near);

    void set_far(double far);

    RenderResult render(bool render_depth,
                        bool render_labeling,
                        bool print_timing);

    void move_and_look_at(double from_x, double from_y, double from_z,
                          double obj_x, double obj_y, double obj_z);

private:
    smart_pointer::shared_ptr<CameraImpl> impl_;
};

/*********************************** World ************************************/
class World {
public:
    World(double gravity, double timestep);

    void remove_object(const Object& obj);

    void clean_everything();

    Thingy load_thingy(const std::string& fn,
                       const Pose& pose,
                       double scale,
                       double mass,
                       int color,
                       bool decoration_only);

    Object load_urdf(const std::string& fn,
                     const Pose& pose,
                     bool fixed_base,
                     bool self_collision);

    std::vector<Object> load_mjcf(const std::string& fn);

    double ts();

    bool step(int repeat);

    Camera new_camera_free_float(int camera_res_w, int camera_res_h,
                                 const std::string& camera_name);

    void set_glsl_path(const std::string& dir);

private:
    smart_pointer::shared_ptr<WorldImpl> impl_;
};

} // roboschool
