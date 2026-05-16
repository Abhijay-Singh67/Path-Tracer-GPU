bool hitScene(ray r, inout hit_record rec){
    bool anyHit = false;
    Interval ray_t;
    ray_t.mn = 0.001f;
    ray_t.mx = INF;
    
    for(int i = 0; i < spheres.length(); i++){
        if(hitSphere(spheres[i].center.xyz, spheres[i].center.w, r, ray_t, rec)){
            ray_t.mx = rec.t;
            int material_index = int(spheres[i].extra.x);
            rec.albedo = materials[material_index].albedo.xyz;
            rec.mat_type = int(materials[material_index].albedo.w);
            rec.fuzz = materials[material_index].extra.x;
            rec.ri = materials[material_index].extra.y;
            anyHit = true;
        }
    }

    return anyHit;
}

vec3 ray_color(in ray r){
    vec3 radiance = vec3(0.0f);
    vec3 throughput = vec3(1.0f);

    for(int bounce = 0; bounce < MAX_BOUNCES; bounce++){
        //Probabilistically kill low-throughput paths
        if (bounce > 3) {  // give every path at least 3 bounces
            float p = max(throughput.r, max(throughput.g, throughput.b));
            if (random_double() > p) break;
            throughput /= p;  // unbiased compensation
        }
        hit_record rec;
        if(hitScene(r, rec)){
            ray scattered;
            vec3 attenuation;
            bool didScatter = false;
            if(rec.mat_type == 0){
                //Lambertian Material
                didScatter = scatterLambertian(r, rec, attenuation, scattered);
            }else if(rec.mat_type == 1){
                //Metal Material
                didScatter = scatterMetal(r, rec, attenuation, scattered);
            }else if(rec.mat_type == 2){
                //Dielectric Material
                didScatter = scatterDielectric(r, rec, attenuation, scattered);
            }

            if(!didScatter) break; //when surfaces absorb

            //updates
            r = scattered;
            throughput *= attenuation;
        }else{
            float a = 0.5*(r.direction.y + 1.0);
            vec3 sky = (1.0 - a)*vec3(1.0f, 1.0f, 1.0f) + a*vec3(0.5f, 0.7f, 1.0f);
            radiance += throughput * sky;
            break;
        }
    }
    return radiance;
}

ray generateCameraRay(vec4 frag){
    vec2 offset = vec2(random_double(), random_double()) - 0.5f;
    vec2 uv = frag.xy + offset;
    uv.x /= WIDTH;
    uv.y /= HEIGHT;
    uv = uv * 2.0f - 1.0f;
    uv.x *= float(WIDTH)/ float(HEIGHT);
    uv *= tan(fov * 0.5);

    vec3 pixel_target = camPosition.xyz
     + focus_dist * cameraForward.xyz
      + uv.x * focus_dist * cameraRight.xyz
       + uv.y * focus_dist * cameraUp.xyz;

    //Choose random origin on the defocus disk
    vec3 ray_origin;
    if (defocus_angle <= 0.0){
        ray_origin = camPosition.xyz;
    } else {
        float defocus_radius = focus_dist * tan(defocus_angle * 0.5);
        //analytical disk sampling
        float r = sqrt(random_double()) * defocus_radius;
        float theta = 6.28318530718 * random_double();
        ray_origin = camPosition.xyz
                   + r * cos(theta) * cameraRight.xyz
                   + r * sin(theta) * cameraUp.xyz;
    }

    ray r;
    r.origin = ray_origin;
    r.direction = normalize(pixel_target - ray_origin);
    return r;
}