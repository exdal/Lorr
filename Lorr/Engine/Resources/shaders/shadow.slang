module shadow;

import std;
import scene;

// TODO: Move this to color, yes i could actually do this instead of writing this text
// but i am SO FUCKING LAZY
func hsv_to_rgb(f32x3 hsv) -> f32x3 {
    let n = f32x3(5.0, 3.0, 1.0);
    let k = (n + hsv.x / 1.0471975512) % 6.0;
    return hsv.z - hsv.z * hsv.y * max(f32x3(0.0), min(k, min(4.0 - k, f32x3(1.0))));
}
