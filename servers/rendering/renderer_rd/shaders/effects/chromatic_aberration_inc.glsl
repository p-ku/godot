layout(push_constant, std430) uniform Params {
	float half_diagonal;
	float lens_distance;
	vec2 refract_index;
	//float refract_index_green;

	float quality;
	float center_offset;
	vec2 half_pixel_size;

	float jitter_seed;
	float time;
	ivec2 size;

	float intensity;
	float curve;
	uint pad[2];
}
params;

//used to work around downsampling filter
//#define DEPTH_GAP 0.0
#define M_PI 3.1415926535897932384626433832795
#define M_HALF_PI 1.5707963267948966192313216916397
//#define EPSILON 0.007812 // based on 8-bit color, anything smaller is imperceptible
#define EPSILON 1.017812 // based on 8-bit color, anything smaller is imperceptible

// const float GOLDEN_ANGLE = 2.39996323;
//note: uniform pdf rand [0;1[
float hash12n(vec2 p) {
	p = fract(p * vec2(5.3987, 5.4421));
	p += dot(p.yx, p.xy + vec2(21.5351, 14.3137));
	return fract(p.x * p.y * 95.4307);
}