//Global Variables
const float INF = 1e30 ;
uint rngState;

struct ray{
    vec3 origin;
    vec3 direction;
};

struct Interval{
    float mn;
    float mx;
};

struct hit_record{
    vec3 normal;
    vec3 hit_point;
    float t;
    vec3 albedo;
    int mat_type; //not really used as of now
};

struct GPUSphere{
    vec4 center;
    vec4 albedo;
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