vec3 sampleEnvMap(vec3 dir){
    // Convert direction to equirectangular UVs
    // Convention: Y up, X right, Z toward camera (camera looks down -Z initially)
    float u = atan(-dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
    float v = acos(clamp(dir.y, -1.0, 1.0)) / 3.14159265;
    
    vec3 envColor = texture(envMap, vec2(u, v)).rgb * envIntensity;

    //Clamp to prevent fireflies
    return min(envColor, vec3(10.0f));
}

bool hitScene(ray r, inout hit_record rec){
    bool anyHit = false;
    Interval ray_t;
    ray_t.mn = 0.001f;
    ray_t.mx = INF;
    
    //Hit Detection using BVH
    int stack[32]; // Max limit of the BVH Stack
    int stack_top = -1;

    int node = 0;
    if (hitAABB(bvhNodes[node].bbox_min.xyz, bvhNodes[node].bbox_max.xyz, r, ray_t) >= ray_t.mx) return false;
    stack[++stack_top] = node;

    while (stack_top >= 0){
        int curr = stack[stack_top--];
        //if hit lets check if we are a leaf
        if(bvhNodes[curr].data.z == 1){ //its a leaf
            //Lets check all the primitives for hits 
            int first_ref = bvhNodes[curr].data.x;
            int ref_count = bvhNodes[curr].data.y;
            for(int i = 0; i < ref_count; i++){
                int prim_type = primRefs[first_ref + i].data.x;
                int prim_index = primRefs[first_ref + i].data.y;
                bool hit;
                int material_index;
                if(prim_type == 0){ //Sphere
                    hit = hitSphere(spheres[prim_index].center.xyz, spheres[prim_index].center.w, r, ray_t, rec);
                    material_index = int(spheres[prim_index].extra.x);
                }else if(prim_type == 1){ //Quad
                    hit = hitQuad(quads[prim_index].Q.xyz, quads[prim_index].u.xyz, quads[prim_index].v.xyz, r, ray_t, rec);
                    material_index = int(quads[prim_index].Q.w);
                }else if(prim_type == 2){ //Triangles
                    hit = hitTriangle(
                        vertices[indices[prim_index].index.x],
                        vertices[indices[prim_index].index.y],
                        vertices[indices[prim_index].index.z],
                        r, ray_t, rec
                        );
                    material_index = indices[prim_index].index.w;
                }else if(prim_type == 3){ //Medium Sphere
                    hit = hitMediumSphere(mediumSpheres[prim_index], r, ray_t, rec);
                }
                if(!hit) continue;
                ray_t.mx = rec.t;
                anyHit = true;
                if (prim_type == 3) continue;
                rec.albedo = materials[material_index].albedo.xyz;
                int texture_id = int(materials[material_index].extra.w);
                if(texture_id > 0){
                    rec.albedo = texture(textures, vec3(rec.u, rec.v, float(texture_id - 1))).xyz;
                }
                rec.mat_type = int(materials[material_index].albedo.w);
                rec.fuzz = materials[material_index].extra.x;
                rec.ri = materials[material_index].extra.y;
                rec.intensity = materials[material_index].extra.z;
                rec.absorption_coeff = materials[material_index].absorption.xyz;
            }
        }else{
            //Push based on the distance of BVH
            int left = bvhNodes[curr].data.x;
            int right = bvhNodes[curr].data.y;
            float t_near_left = hitAABB(bvhNodes[left].bbox_min.xyz, bvhNodes[left].bbox_max.xyz, r, ray_t);
            float t_near_right = hitAABB(bvhNodes[right].bbox_min.xyz, bvhNodes[right].bbox_max.xyz, r, ray_t);

            if (t_near_left >= ray_t.mx && t_near_right >= ray_t.mx) continue;

            if(t_near_left < t_near_right){
                if (t_near_right < ray_t.mx) stack[++stack_top] = right;
                if (t_near_left < ray_t.mx) stack[++stack_top] = left;
            }else{
                if (t_near_left < ray_t.mx) stack[++stack_top] = left;
                if (t_near_right < ray_t.mx) stack[++stack_top] = right;
            }
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
            }else if(rec.mat_type == 3){
                //Emissive Material
                radiance += throughput * emission(rec);
                break;
            }else if(rec.mat_type == 4){
                didScatter = scatterMedium(r, rec, attenuation, scattered);
            }

            if(!didScatter) break; //when surfaces absorb

            //updates
            r = scattered;
            throughput *= attenuation;
        }else{
            radiance += throughput * ((screenData.w == 1)? sampleEnvMap(r.direction): background.xyz * background.w);
            break;
        }
    }
    return radiance;
}

ray generateCameraRay(vec4 frag){
    //Some declarations to use
    float WIDTH = screenData.x;
    float HEIGHT = screenData.y;
    float fov = cameraData.x;
    float defocus_angle = cameraData.y;
    float focus_dist = cameraData.z;

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