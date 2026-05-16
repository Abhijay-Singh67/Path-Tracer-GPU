//Global Variables
const float INF = 1e30 ;
uint rngState;
const int MAX_BOUNCES = 500;
const float ATMOSPHERE_RI = 1.0f;

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
    float fuzz;
    float ri;
    int mat_type;
    bool front_face;
};

struct GPUSphere{
    vec4 center;
    vec4 albedo;
    vec4 extra;
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

vec3 random_unit_vector() {
    float z = 1.0 - 2.0 * random_double();  
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = 6.28318530718 * random_double();    
    return vec3(r * cos(phi), r * sin(phi), z);
}