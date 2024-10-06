layout(push_constant, std430) uniform Params {
	ivec2 size;
	ivec2 full_size;

	vec2 pixel_size;
	vec2 center;

	float jitter_seed;
	float max_samples;
	float edge_factor;

	float minimum_distance;
	uint pad[8];
}
params;

#define M_PI 3.1415926535897932384626433832795
#define M_HALF_PI 1.5707963267948966192313216916397
//#define EPSILON 0.0078125 // based on 8-bit color (2 / 256), anything smaller is imperceptible
#define EPSILON 0.00390625 // based on 8-bit color (1 / 256), rounded down from 0.00390625

//#define EPSILON 1.017812 // based on ?
#define EPSILON3 0.339270666666667

const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);

#ifdef MODE_CA_PROCESS_JITTER
//note: uniform pdf rand [0;1[
float hash12n(vec2 p) {
	p = fract(p * vec2(5.3987, 5.4421));
	p += dot(p.yx, p.xy + vec2(21.5351, 14.3137));
	return fract(p.x * p.y * 95.4307);
}
// float gradientNoise(vec2 pos) {
// 	return fract(52.9829189 * fract(dot(pos, vec2(0.06711056, 0.00583715))));
// }

float gradientNoise(const vec2 pos) {
	return fract(magic.z * fract(dot(pos, magic.xy)));
}
#endif
