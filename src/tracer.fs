#version 420 core

in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D prevFrameTexture; //not used as of now

layout (std140, binding = 0) uniform Camera{
    vec4 camPosition;
    vec4 cameraRight;
    vec4 cameraUp;
    vec4 cameraForward;
    int WIDTH;
    int HEIGHT;
    float fov;
    float _pad;
};

struct ray{
    vec3 origin;
    vec3 direction;
};

struct hit_record{
    vec3 normal;
    vec3 hit_point;
    float t;
};

//Function Prototypes
vec3 ray_color(ray r);
bool hitSphere(ray r, out hit_record rec);

void main() {
    //We first construct a ray
    ray r;
    r.origin = camPosition.xyz;
    vec2 uv = TexCoord*2.0 - 1.0;
    uv.x *= float(WIDTH)/ float(HEIGHT);
    float halfHeight = tan(fov * 0.5);
    uv *= halfHeight;
    r.direction = normalize(uv.x * cameraRight.xyz + uv.y * cameraUp.xyz + cameraForward.xyz);
    FragColor = vec4(ray_color(r),1.0f);
}

vec3 ray_color(ray r){
    hit_record rec;
    if (hitSphere(r,rec)){
        return 0.5 * (rec.normal + vec3(1.0f));
    }
    float a = 0.5*(r.direction.y + 1.0);
    return (1.0 - a)*vec3(1.0f, 1.0f, 1.0f) + a*vec3(0.5f, 0.7f, 1.0f);
}

bool hitSphere(ray r, out hit_record rec){
    vec3 sphereCenter = vec3(0.0f, 0.0f, -1.0f); //Hardcoded for now
    float radius = 0.5f; //Hardcoded for now
    vec3 oc = sphereCenter - r.origin;
    float a = dot(r.direction, r.direction);
    float h = dot(r.direction, oc);
    float c = dot(oc, oc) - radius * radius;

    float discriminant = h*h - a*c;
    if(discriminant < 0){
        return false;
    }
    float sqrtd = sqrt(discriminant);
    float root = (h - sqrtd) / a; // for now we just take the smaller root
    if (root < 0.0f) return false;
    //update the records
    rec.t = root;
    rec.hit_point = r.origin + root * r.direction;
    rec.normal = (rec.hit_point - sphereCenter) / radius;
    //we'll just return true for now
    return true;
}