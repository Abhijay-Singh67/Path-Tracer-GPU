bool scatterLambertian(in ray r,in hit_record rec, inout vec3 attenuation, inout ray scattered){
    vec3 scatter_direction = rec.normal + random_unit_vector();
    if(length(scatter_direction) < 1e-8){
        scatter_direction = rec.normal;
    }

    //Checking if the scatter went below the triangle plane
    if(dot(scatter_direction, rec.geom_normal) <= 0.0) return false;

    scattered.origin = rec.hit_point;
    scattered.direction = scatter_direction;
    attenuation = rec.albedo;
    return true;
}

bool scatterMetal(in ray r, in hit_record rec, inout vec3 attenuation, inout ray scattered){
    float scatterProb = 0.8f;
    vec3 reflected = reflect(r.direction, rec.normal);
    reflected = normalize(reflected) + (min(rec.fuzz, 1.0f) * random_unit_vector());

    // Two checks: the fuzz check AND the geometric-horizon check
    if (dot(reflected, rec.normal) <= 0.0) return false;       
    if (dot(reflected, rec.geom_normal) <= 0.0) return false;

    scattered.origin = rec.hit_point;
    scattered.direction = normalize(reflected);
    attenuation = rec.albedo / scatterProb;
    return true;
}

float reflectance(float cosine, float refraction_index){
    //Use Schlick's approximation for reflectance
    float r0 = (1-refraction_index) / (1+refraction_index);
    r0=r0*r0;
    return r0 + (1-r0)*pow((1-cosine),5);
}

vec3 refract(vec3 uv, vec3 n, float etai_over_etat){
    float cos_theta = min(dot(-uv,n),1.0);
    vec3 r_out_perp = etai_over_etat*(uv+cos_theta*n);
    vec3 r_out_parallel = -sqrt(abs(1.0-dot(r_out_perp, r_out_perp)))*n;
    return r_out_perp + r_out_parallel;
}

bool scatterDielectric(in ray r, in hit_record rec, inout vec3 attenuation, inout ray scattered){
    attenuation = rec.albedo;
    float ri = rec.front_face ? (ATMOSPHERE_RI / rec.ri) : rec.ri / ATMOSPHERE_RI;

    vec3 unit_direction = normalize(r.direction);
    float cos_theta = min(dot(-unit_direction, rec.normal), 1.0);
    float sin_theta = sqrt(1.0 - cos_theta*cos_theta);

    bool cannot_refract = ri * sin_theta > 1.0;
    vec3 direction;
    bool is_reflection;

    if(cannot_refract || reflectance(cos_theta, ri) > random_double()){
        direction = reflect(unit_direction, rec.normal);
        is_reflection = true;
    }else{
        direction = refract(unit_direction, rec.normal, ri);
        is_reflection = false;
    }

    // Geometric-horizon check, but the direction depends on which case we're in
    if (is_reflection) {
        // Reflected ray should stay on the incident side of geom_normal
        if (dot(direction, rec.geom_normal) <= 0.0) return false;
    } else {
        // Refracted ray should cross to the other side of geom_normal
        if (dot(direction, rec.geom_normal) >= 0.0) return false;
    }

    scattered.origin = rec.hit_point;
    scattered.direction = direction;

    return true;
}