// layout(push_constant, std430) uniform Params {
// 	float half_diagonal;
// 	float samples;
// 	float edge_amount;
// 	float linear_amount;

// 	vec2 center;
// 	float minimum_distance;
// 	float desaturation;

// 	float horizontal_smear;
// 	float vertical_smear;
// 	vec2 half_pixel_size;

// 	float jitter_amount;
// 	ivec2 size;
// 	float jitter_seed;

// 	//	uint pad[8];
// }
// params;
#ifdef MODE_CA_SPECTRUM
layout(push_constant, std430) uniform Params {
	ivec2 size;
	float desaturation;
	uint pad;
}
params;
#endif
#ifdef MODE_CA_REFRACTION_BOX
layout(push_constant, std430) uniform Params {
	ivec2 half_size;
	ivec2 full_size;

	float edge_factor;
	float linear_factor;
	float minimum_distance;
	bool landscape;

	vec2 center;
	vec2 center_dir;

	int pixel_count;
	int spiral_pixel_count;
	uint max_samples;
	uint pad;
}
params;
#endif
#ifdef MODE_CA_PROCESS
layout(push_constant, std430) uniform Params {
	ivec2 half_size;
	ivec2 full_size;

	vec2 pixel_size;
	float min_uv_delta;
	float jitter_amount;

	vec2 jitter_seed;
	float max_samples;
	uint pad;
}
params;
#endif
// layout(push_constant, std430) uniform Params {
// 	ivec2 half_size;
// 	ivec2 full_size;

// 	int max_samples;
// 	float edge_factor;
// 	float linear_factor;
// 	float minimum_distance;

// 	vec2 center;
// 	float desaturation;
// 	float diagonal;

// 	int pixel_count;
// 	int spiral_pixel_count;
// 	vec2 half_pixel_size;

// 	float jitter_amount;
// 	float jitter_seed;
// 	bool landscape;
// 	float time;

// 	//	uint pad[12];
// }
// params;

//test
//used to work around downsampling filter
//#define DEPTH_GAP 0.0
#define M_PI 3.1415926535897932384626433832795
#define M_HALF_PI 1.5707963267948966192313216916397
//#define EPSILON 0.0078125 // based on 8-bit color (2 / 256), anything smaller is imperceptible
#define EPSILON 1.017812 // based on ?
#define EPSILON3 0.339270666666667

// #define EPSILON 2.0

// const float GOLDEN_ANGLE = 2.39996323;
const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
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

// https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
vec3 interleaved_gradient_noise(vec2 pos) {
	const vec3 magic2 = vec3(0.06711056f, 0.00583715f, 52.9829189f);
	float res = fract(magic2.z * fract(dot(pos, magic2.xy))) * 2.0 - 1.0;
	return vec3(res, -res, res) / 255.0;
}

// vec2 rotate_gradient(vec2 pos, vec2) {
// 	float noise = 2.0 * M_PI * gradientNoise(pos);
// 	vec2 rotation = vec2(cos(noise), sin(noise));
// 	mat2 rotation_matrix = mat2(rotation.x, rotation.y, -rotation.y, rotation.x);
// 	return
// }
