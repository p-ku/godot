layout(push_constant, std430) uniform Params {
	ivec2 size;
	//	float z_far;
	//	float z_near;
	//
	//	bool orthogonal;
	//	float blur_size;
	//	float blur_scale;
	//	int blur_steps;
	//
	//	bool blur_near_active;
	//	float blur_near_begin;
	//	float blur_near_end;
	//	bool blur_far_active;
	//
	//	float blur_far_begin;
	//	float blur_far_end;
	//	bool second_pass;
	//	bool half_size;
	//
	//	bool use_jitter;
	//	float jitter_seed;
	//	bool use_physical_near;
	//	bool use_physical_far;
	//
	//	float blur_size_near;
	//	float blur_size_far;
	float ca_axial;
	float ca_transverse;

	float max_distance;
	float lens_center_line;
	float sensor_half_diagonal;
	float angle_factor;

	float diagonal_half_fov;
	float refract_index_red;
	float refract_index_green;
	float refract_index_blue;

	float apothem;
	float focal_length;
	uint pad[2];

	//	float image_distance;
	//	float image_distance_red;
	//	float image_distance_green;
	//	float image_distance_blue;
	//	float focal_length;

	//	float lens_distance;
	//	float lens_distance_red;
	//	float lens_distance_green;
	//	float lens_distance_blue;

	//float sensor_size;
	//	uint pad[2];

	//	uint pad;
}
params;

//used to work around downsampling filter
//#define DEPTH_GAP 0.0
#define M_PI 3.1415926535897932384626433832795
#define M_HALF_PI 1.5707963267948966192313216916397

// const float GOLDEN_ANGLE = 2.39996323;
//note: uniform pdf rand [0;1[
//float hash12n(vec2 p) {
//	p = fract(p * vec2(5.3987, 5.4421));
//	p += dot(p.yx, p.xy + vec2(21.5351, 14.3137));
//	return fract(p.x * p.y * 95.4307);
//}