bool scatterLambertian(in ray r,in hit_record rec, inout vec3 attenuation, inout ray scattered){
    vec3 scatter_direction = rec.normal + random_unit_vector();
    if(length(scatter_direction) < 1e-8){
        scatter_direction = rec.normal;
    }
    scattered.origin = rec.hit_point;
    scattered.direction = scatter_direction;
    attenuation = rec.albedo;
    return true;
}

bool scatterMetal(in ray r, in hit_record rec, inout vec3 attenuation, inout ray scattered){
    float scatterProb = 0.8f;
    vec3 reflected = reflect(r.direction, rec.normal);
    reflected = normalize(reflected) + (min(rec.fuzz, 1.0f) * random_unit_vector());
    scattered.origin = rec.hit_point;
    scattered.direction = reflected;
    attenuation = rec.albedo / scatterProb;
    return (dot(scattered.direction, rec.normal) > 0);
}