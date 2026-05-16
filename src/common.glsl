struct ray{
    vec3 origin;
    vec3 direction;
};

struct hit_record{
    vec3 normal;
    vec3 hit_point;
    float t;
};

uint pcg_hash(uint x){
    x = x * 747796405u + 2891336453u;
    uint word = ((x >> (( x >> 28u) + 4u)) ^ x) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand(inout uint state){
    state = pcg_hash(state);
    return float(state) / 4294967295.0;
}

float random_double(){
    return rand(rngState);
}