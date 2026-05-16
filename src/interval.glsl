Interval interval_tight(Interval a, Interval b){
    float mn = a.mn <= b.mn ? a.mn : b.mn;
    float mx = a.mx >= b.mx ? a.mx : b.mx;
    Interval i;
    i.mn = mn;
    i.mx = mx;
    return i; 
}

bool interval_contains(Interval i, float x){
    return i.mn <= x && x <= i.mx;
}

bool interval_surrounds(Interval i, float x){
    return i.mn < x && x < i.mx;
}

float interval_clamp(Interval i, float x){
    if(x < i.mn) return i.mn;
    if(x > i.mx) return i.mx;
    return x;
}