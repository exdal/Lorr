implementing std;

public namespace std {
// Unit vectors octahedral encoding
// "Survey of Efficient Representations for Independent Unit Vectors"
// http://jcgt.org/published/0003/02/01
func octahedral_decode_signed(f32x2 v) -> f32x3 {
    var n = f32x3(v.xy, 1.0 - abs(v.x) - abs(v.y));
    let t = saturate(-n.z);
    let w = select(n.xy >= 0.0, f32x2(-t), f32x2(t));
    n = f32x3(n.xy + w, n.z);
    return normalize(n);
}

public func octahedral_decode(f32x2 v) -> f32x3 {
    let f = v * 2.0 - 1.0;
    return octahedral_decode_signed(f);
}

public func octahedral_encode(f32x3 v) -> f32x2 {
    var n = v / (abs(v.x) + abs(v.y) + abs(v.z));
    let octahedral_wrap = (1.0 - abs(n.yx)) * select(n.xy > f32x2(0.0), f32x2(1.0), f32x2(-1.0));
    let n_xy = select(n.z >= 0.0, n.xy, octahedral_wrap);
    return n_xy * 0.5 + 0.5;
}

}
