#ifndef AABB_H
#define AABB_H

#include "interval.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "structs.h"

class aabb
{
public:
    interval x, y, z;

    aabb() {} // The default AABB is empty, since intervals are empty by default.

    aabb(const interval &x, const interval &y, const interval &z)
        : x(x), y(y), z(z)
    {
        pad_to_minimums();
    }

    aabb(const glm::vec3 &a, const glm::vec3 &b)
    {
        // Treat the two points a and b as extrema for the bounding box, so we don't require a
        // particular minimum/maximum coordinate order.

        x = (a[0] <= b[0]) ? interval(a[0], b[0]) : interval(b[0], a[0]);
        y = (a[1] <= b[1]) ? interval(a[1], b[1]) : interval(b[1], a[1]);
        z = (a[2] <= b[2]) ? interval(a[2], b[2]) : interval(b[2], a[2]);
        pad_to_minimums();
    }

    aabb(const aabb &box0, const aabb &box1)
    {
        x = interval(box0.x, box1.x);
        y = interval(box0.y, box1.y);
        z = interval(box0.z, box1.z);
        pad_to_minimums();
    }

    // Construct AABB extended to include a point
    aabb(const aabb& box, const glm::vec3& point) {
        x = interval(std::fmin(box.x.min, point.x), std::fmax(box.x.max, point.x));
        y = interval(std::fmin(box.y.min, point.y), std::fmax(box.y.max, point.y));
        z = interval(std::fmin(box.z.min, point.z), std::fmax(box.z.max, point.z));
        pad_to_minimums();
    }

    // Per-axis access to the interval
    const interval& axis_interval(int n) const {
        if (n == 1) return y;
        if (n == 2) return z;
        return x;
    }

    // Surface area for SAH
    float surface_area() const {
        float dx = float(x.size());
        float dy = float(y.size());
        float dz = float(z.size());
        return 2.0f * (dx*dy + dy*dz + dz*dx);
    }

    int longest_axis() const
    {
        // Returns the index of the longest axis of the bounding box.

        if (x.size() > y.size())
            return x.size() > z.size() ? 0 : 2;
        else
            return y.size() > z.size() ? 1 : 2;
    }

    aabb sphere_aabb(const GPUSphere &sphere)
    {
        auto radius = sphere.center.w;
        auto rvec = glm::vec3(radius, radius, radius);
        glm::vec3 center = glm::vec3(sphere.center);
        return aabb(center - rvec, center + rvec);
    }

    aabb quad_aabb(const GPUQuad &quad)
    {
        glm::vec3 Q = glm::vec3(quad.Q);
        glm::vec3 u = glm::vec3(quad.u);
        glm::vec3 v = glm::vec3(quad.v);
        aabb bbox_diagonal1 = aabb(Q, Q + u + v);
        aabb bbox_diagonal2 = aabb(Q + u, Q + v);
        return aabb(bbox_diagonal1, bbox_diagonal2);
    }

    aabb triangle_aabb(const GPUIndex &triangle, const std::vector<GPUVertex> &vertices)
    {
        const glm::vec3 v0 = glm::vec3(vertices[triangle.index.x].position);
        const glm::vec3 v1 = glm::vec3(vertices[triangle.index.y].position);
        const glm::vec3 v2 = glm::vec3(vertices[triangle.index.z].position);

        glm::vec3 mn = glm::min(v0, glm::min(v1, v2));
        glm::vec3 mx = glm::max(v0, glm::max(v1, v2));

        return aabb(mn, mx);
    }

    glm::vec4 sphere_centroid(const GPUSphere &sphere){
        return sphere.center;
    }

    glm::vec4 quad_centroid(const GPUQuad &quad){
        glm::vec3 Q = glm::vec3(quad.Q);
        glm::vec3 u = glm::vec3(quad.u);
        glm::vec3 v = glm::vec3(quad.v);
        glm::vec3 centroid = (Q + u + v) / 2.0f;
        return glm::vec4(centroid, 0.0f);
    }

    glm::vec4 triangle_centroid(const GPUIndex &index, const std::vector<GPUVertex> &vertices){
        glm::vec3 v0 = glm::vec3(vertices[index.index.x].position);
        glm::vec3 v1 = glm::vec3(vertices[index.index.y].position);
        glm::vec3 v2 = glm::vec3(vertices[index.index.z].position);
        glm::vec3 centroid = (v0 + v1 + v2) / 3.0f;
        return glm::vec4(centroid, 0.0f);
    }

    static const aabb empty, universe;

private:
    void pad_to_minimums()
    {
        double delta = 0.0001;
        if (x.size() < delta)
            x = x.expand(delta);
        if (y.size() < delta)
            y = y.expand(delta);
        if (z.size() < delta)
            z = z.expand(delta);
    }
};

const aabb aabb::empty = aabb(interval::empty, interval::empty, interval::empty);
const aabb aabb::universe = aabb(interval::universe, interval::universe, interval::universe);

aabb operator+(const aabb &bbox, const glm::vec3 &offset)
{
    return aabb(bbox.x + offset.x, bbox.y + offset.y, bbox.z + offset.z);
}

aabb operator+(const glm::vec3 &offset, const aabb &bbox)
{
    return bbox + offset;
}

#endif