struct ray{
    vec3 origin;
    vec3 direction;
};

struct hit_record{
    vec3 normal;
    vec3 hit_point;
    float t;
};

