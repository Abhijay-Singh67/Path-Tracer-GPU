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

bool hitMediumSphere(GPUMediumSphere ms, ray r, Interval ray_t, inout hit_record rec) {
    vec3 center = ms.center.xyz;
    float radius = ms.center.w;
    
    // Standard sphere intersection — get both roots
    vec3 oc = center - r.origin;
    float a = dot(r.direction, r.direction);
    float h = dot(r.direction, oc);
    float c = dot(oc, oc) - radius * radius;
    
    float discriminant = h*h - a*c;
    if (discriminant < 0) return false;
    float sqrtd = sqrt(discriminant);
    
    float t_enter = (h - sqrtd) / a;
    float t_exit  = (h + sqrtd) / a;
    
    // Clamp to ray interval
    if (t_enter < ray_t.mn) t_enter = ray_t.mn;
    if (t_exit  > ray_t.mx) t_exit  = ray_t.mx;
    if (t_enter >= t_exit) return false;
    if (t_enter < 0.0) t_enter = 0.0;
    
    // Sample scatter distance via Beer-Lambert
    float density = ms.albedo_density.w;
    float distance_inside = t_exit - t_enter;
    float u = max(random_double(), 1e-8);
    float hit_distance = -log(u) / density;
    
    if (hit_distance > distance_inside) {
        // Ray passes through medium without scattering — report no hit,
        // BVH will continue searching for surfaces behind
        return false;
    }
    
    // Commit a scatter event at the sampled point
    rec.t = t_enter + hit_distance;
    rec.hit_point = r.origin + rec.t * r.direction;
    rec.normal = vec3(1.0, 0.0, 0.0);  // arbitrary, isotropic scatter ignores it
    rec.geom_normal = rec.normal;
    rec.front_face = true;
    rec.albedo = ms.albedo_density.xyz;
    rec.mat_type = 4;  // signal "this is a medium scatter"
    return true;
}