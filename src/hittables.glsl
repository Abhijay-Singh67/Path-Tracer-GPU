void get_sphere_uv(vec3 p, inout float u, inout float v){
    float theta = acos(-p.y);
    float phi = atan(-p.z,p.x) + pi;

    u = phi / (2*pi);
    v = theta / pi;
}

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
    vec3 outwardNormal = (rec.hit_point - sphereCenter) / radius;
    rec.front_face = dot(r.direction, outwardNormal) < 0;
    rec.normal = rec.front_face ? outwardNormal : -outwardNormal;
    get_sphere_uv(outwardNormal, rec.u, rec.v);
    return true;
}

bool is_interior(float a, float b, inout hit_record rec){
    Interval unit_interval;
    unit_interval.mn = 0.0f;
    unit_interval.mx = 1.0f;

    if(!interval_contains(unit_interval, a) || !interval_contains(unit_interval, b)){
        return false;
    }

    rec.u = a;
    rec.v = b;

    return true;
}

bool hitQuad(vec3 Q, vec3 u, vec3 v, ray r, Interval ray_t, inout hit_record rec){
    //Some pre calculations
    vec3 n = cross(u, v);
    vec3 normal = normalize(n);
    float D = dot(normal, Q);
    vec3 w = n / dot(n,n);

    //Hit calculations
    float denom = dot(normal, r.direction);
    if (abs(denom) < 1e-8) return false;

    float t = (D - dot(normal, r.origin))/denom;
    if (!interval_contains(ray_t, t)) return false;

    vec3 intersection = r.origin + t * r.direction;
    vec3 planar_hitpt_vector = intersection - Q;
    float alpha = dot(w, cross(planar_hitpt_vector, v));
    float beta = dot(w, cross(u, planar_hitpt_vector));

    if (!is_interior(alpha, beta, rec)){
        return false;
    }

    rec.t = t;
    rec.hit_point = intersection;
    rec.front_face = dot(r.direction, normal) < 0;
    rec.normal = rec.front_face ? normal : -normal;

    return true;
}