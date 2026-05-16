bool hitSphere(ray r, Interval ray_t, out hit_record rec);
vec3 ray_color(ray r);
ray generateCameraRay(vec2 texCoord);
Interval interval_tight(Interval a, Interval b);
bool interval_contains(Interval i, float x);
bool interval_surrounds(Interval i, float x);
float clamp(Interval i, float x);