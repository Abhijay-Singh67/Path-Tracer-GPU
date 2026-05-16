bool hitScene(ray r, inout hit_record rec){
    bool anyHit = false;
    Interval ray_t;
    ray_t.mn = 0.001f;
    ray_t.mx = INF;
    
    for(int i = 0; i < spheres.length(); i++){
        if(hitSphere(spheres[i].center.xyz, spheres[i].center.w, r, ray_t, rec)){
            ray_t.mx = rec.t;
            rec.albedo = spheres[i].albedo.xyz;
            rec.mat_type = int(spheres[i].albedo.w);
            rec.fuzz = spheres[i].extra.x;
            rec.ri = spheres[i].extra.y;
            anyHit = true;
        }
    }

    return anyHit;
}

vec3 ray_color(in ray r){
    vec3 radiance = vec3(0.0f);
    vec3 throughput = vec3(1.0f);

    for(int bounce = 0; bounce < MAX_BOUNCES; bounce++){
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

    ray r;
    r.origin = camPosition.xyz;
    r.direction = normalize(uv.x * cameraRight.xyz + uv.y * cameraUp.xyz + cameraForward.xyz);
    return r;
}