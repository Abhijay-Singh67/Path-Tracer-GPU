AABB aabb_using_points(vec3 a, vec3 b){
    AABB aabb;
    aabb.x = (a[0] <= b[0]) ? Interval(a[0], b[0]) : Interval(b[0], a[0]);
    aabb.y = (a[1] <= b[1]) ? Interval(a[1], b[1]) : Interval(b[1], a[1]);
    aabb.z = (a[2] <= b[2]) ? Interval(a[2], b[2]) : Interval(b[2], a[2]);
    return aabb;
}

Interval axis_interval(AABB box, int n){
    if (n==1) return box.y;
    if (n==2) return box.z;
    return box.x;
}

float hitAABB(vec3 bbox_min, vec3 bbox_max, ray r, Interval ray_t){
    AABB aabb = aabb_using_points(bbox_min, bbox_max);
    float t_near = ray_t.mn;
    float t_far = ray_t.mx;

    for(int axis = 0; axis < 3; axis++){
        Interval ax = axis_interval(aabb, axis);
        float adinv = 1.0 / r.direction[axis];

        float t0 = (ax.mn - r.origin[axis]) * adinv;
        float t1 = (ax.mx - r.origin[axis]) * adinv;

        if(t0 > t1){
            float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }

        t_near = max(t_near, t0);
        t_far = min(t_far, t1);

        if (t_far <= t_near){
            return INF;
        }
    }

    return t_near;
}