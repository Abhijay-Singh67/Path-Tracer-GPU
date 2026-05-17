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
    rec.geom_normal = rec.normal;
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
    rec.geom_normal = rec.normal;

    return true;
}

bool hitTriangle(GPUVertex vert0, GPUVertex vert1, GPUVertex vert2, ray r, Interval ray_t, inout hit_record rec){
    //Vertex Positions
    vec3 v0 = vert0.position.xyz;
    vec3 v1 = vert1.position.xyz;
    vec3 v2 = vert2.position.xyz;

    vec3 E1 = v1 - v0;
    vec3 E2 = v2 - v0;
    vec3 pvec = cross(r.direction, E2);
    float det = dot(E1, pvec);

    if(abs(det) < 1e-8) return false;
    float invDet = 1.0 / det;

    vec3 T = r.origin - v0;
    float u = dot(T, pvec) * invDet;
    if(u < 0.0f || u > 1.0f) return false;

    vec3 qvec = cross(T, E1);
    float v = dot(r.direction, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;

    float t = dot(E2, qvec) * invDet;
    
    if(!interval_contains(ray_t, t)) return false;

    rec.t = t;
    rec.hit_point = r.origin + t * r.direction;

    //Interpolate the normal from vertex normals
    vec3 n0 = vert0.normal.xyz;
    vec3 n1 = vert1.normal.xyz;
    vec3 n2 = vert2.normal.xyz;
    vec3 shading_normal = normalize((1.0 - u - v) * n0 + u * n1 + v * n2);

    //Use geometric face normal for front_face determination
    vec3 normal = normalize(cross(E1, E2));
    rec.front_face = dot(r.direction,normal) < 0;

    rec.normal = rec.front_face ? shading_normal : -shading_normal;
    rec.geom_normal = rec.front_face ? normal : -normal;

    rec.u = u;
    rec.v = v;

    return true;
}