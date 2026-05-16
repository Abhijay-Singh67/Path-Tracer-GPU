vec3 ray_color(ray r){
    hit_record rec;
    if (hitSphere(r,rec)){
        return 0.5 * (rec.normal + vec3(1.0f));
    }
    float a = 0.5*(r.direction.y + 1.0);
    return (1.0 - a)*vec3(1.0f, 1.0f, 1.0f) + a*vec3(0.5f, 0.7f, 1.0f);
}

ray generateCameraRay(vec2 texCoord){
    vec2 uv = texCoord * 2.0 - 1.0;
    uv.x *= float(WIDTH)/ float(HEIGHT);
    uv *= tan(fov * 0.5);

    ray r;
    r.origin = camPosition.xyz;
    r.direction = normalize(uv.x * cameraRight.xyz + uv.y * cameraUp.xyz + cameraForward.xyz);
    return r;
}