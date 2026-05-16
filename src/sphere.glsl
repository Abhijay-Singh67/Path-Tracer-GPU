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