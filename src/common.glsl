//Global Variables
const float INF = 1e30 ;
uint rngState;
const int MAX_BOUNCES = 50;
const float ATMOSPHERE_RI = 1.0f;
const float pi = 3.1415926535897932385f;

struct ray{
    vec3 origin;
    vec3 direction;
};

struct Interval{
    float mn;
    float mx;
};

struct hit_record{
    vec3 normal; //shading normal
    vec3 geom_normal;
    vec3 hit_point;
    float t;
    vec3 albedo;
    float fuzz;
    float ri;
    float intensity; //used by lights
    vec3 absorption_coeff; //used for glass tinting
    int mat_type;
    bool front_face;
    float u; //surface coords for textures
    float v;
};

struct Material{
    vec4 albedo;
    vec4 extra;
    vec4 absorption;
};

struct GPUSphere{
    vec4 center;
    vec4 extra;
};

struct GPUQuad{
    vec4 Q;
    vec4 u;
    vec4 v;
};

struct GPUVertex{
    vec4 position;
    vec4 normal;
};

struct GPUIndex{
    ivec4 index;
};

struct AABB{
    Interval x;
    Interval y;
    Interval z;
};

struct BVHNode{
    vec4 bbox_min;
    vec4 bbox_max;
    ivec4 data;
};

struct PrimRef{
    ivec4 data;
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