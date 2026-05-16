vec3 ray_color(ray r){
    hit_record rec;
    if (hitSphere(r,rec)){
        return 0.5 * (rec.normal + vec3(1.0f));
    }
    float a = 0.5*(r.direction.y + 1.0);
    return (1.0 - a)*vec3(1.0f, 1.0f, 1.0f) + a*vec3(0.5f, 0.7f, 1.0f);
}

ray generateCameraRay(vec2 texCoord, vec4 frag){
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