bool hitSphere(vec3 sphereCenter, float radius, ray r, Interval ray_t, inout hit_record rec){
    vec3 oc = sphereCenter - r.origin;
    float a = dot(r.direction, r.direction);
    float h = dot(r.direction, oc);
    float c = dot(oc, oc) - radius * radius;

    float discriminant = h*h - a*c;
    if(discriminant < 0){
        return false;
    }
    float sqrtd = sqrt(discriminant);
    float root = (h - sqrtd) / a;
    if (!interval_surrounds(ray_t, root)){
        root = (h + sqrtd) / a;
        if (!interval_surrounds(ray_t, root)){
            return false;
        }
    }
    //update the records
    rec.t = root;
    rec.hit_point = r.origin + root * r.direction;
    rec.normal = (rec.hit_point - sphereCenter) / radius;

    return true;
}