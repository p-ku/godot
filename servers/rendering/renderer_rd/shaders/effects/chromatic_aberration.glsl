#[compute]

#version 450

#VERSION_DEFINES

#define BLOCK_SIZE 8

//layout(local_size_x = BLOCK_SIZE, local_size_y = BLOCK_SIZE, local_size_z = 1) in;
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

#ifdef MODE_CA_SPECTRUM
layout(rgba16f, set = 0, binding = 0) uniform restrict writeonly image2D spectrum_image;
#endif

#ifdef MODE_CA_REFRACTION_BOX
layout(rgba32f, set = 0, binding = 0) uniform restrict writeonly image2D refraction_image;
#endif

#ifdef MODE_CA_PROCESS
// layout(rgba16f, set = 0, binding = 0) uniform restrict writeonly image2D ca_image;
layout(set = 0, binding = 0) uniform sampler2D copy_texture;
layout(set = 1, binding = 0) uniform sampler2D refraction_texture;
// layout(set = 2, binding = 0) uniform sampler2D color_texture;
layout(set = 2, binding = 0) uniform sampler2D spectrum_texture;
layout(rgba16f, set = 3, binding = 0) uniform restrict image2D ca_image;
layout(rgba16f, set = 4, binding = 0) uniform restrict writeonly image2D final_image;

#endif

#ifdef MODE_CA_COMPOSITE
layout(rgba16f, set = 0, binding = 0) uniform restrict image2D final_image;
layout(set = 1, binding = 0) uniform sampler2D ca_texture;
#endif

#include "chromatic_aberration_inc.glsl"

#ifdef MODE_CA_REFRACTION_BOX

int get_spiral_index(const ivec2 pos) {
	const int far_dim = max(pos.x, pos.y);
	// int near_dim = min(pos.x, pos.y);
	const int spiral_dir = far_dim % 2 == 1 ? 1 : -1;

	return far_dim * far_dim + far_dim + spiral_dir * (pos.y - pos.x);
}

#endif

#ifdef MODE_CA_PROCESS

float triangle_wave(const float period, const float t) {
	return 2.0 * abs(t / period - floor(t / period + 0.5));
}

float nrand(const vec2 n) {
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
#endif

void main() {
#ifdef MODE_CA_SPECTRUM
	const ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.size))) { //too large, do nothing
		return;
	}

	if (pos.x == 0) {
		imageStore(spectrum_image, pos, vec4(0.0, 0.0, 0.0, 1.0));
	} else if (pos.x == 1) {
		imageStore(spectrum_image, pos, vec4(1.0, params.desaturation, params.desaturation, 1.0));
	} else if (pos.x == 2) {
		imageStore(spectrum_image, pos, vec4(params.desaturation, 1.0, params.desaturation, 1.0));
	} else if (pos.x == 3) {
		imageStore(spectrum_image, pos, vec4(params.desaturation, params.desaturation, 1.0, 1.0));
	} else if (pos.x == 4) {
		imageStore(spectrum_image, pos, vec4(0.0, 0.0, 0.0, 1.0));
	}

	// if (pos.x == 0) {
	// 	imageStore(spectrum_image, pos, vec4(1.0, params.desaturation, params.desaturation, 1.0));
	// } else if (pos.x == 1) {
	// 	imageStore(spectrum_image, pos, vec4(params.desaturation, 1.0, params.desaturation, 1.0));
	// } else if (pos.x == 2) {
	// 	imageStore(spectrum_image, pos, vec4(params.desaturation, params.desaturation, 1.0, 1.0));
	// }

#endif
#ifdef MODE_CA_REFRACTION_BOX
	const ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.half_size))) { //too large, do nothing
		return;
	}

	const int order = pos.x + pos.y * params.half_size.x;
	ivec2 spiral_pos;
	const int inv_index = params.pixel_count - 1 - order;
	if (inv_index < params.spiral_pixel_count) {
		const int spiral_layer = int(sqrt(inv_index));
		const int index_in_layer = int(inv_index) - spiral_layer * (spiral_layer + 1);

		const int lmax = max(index_in_layer, 0);

		if (lmax <= spiral_layer) {
			spiral_pos.x = spiral_layer - lmax;
			spiral_pos.y = spiral_layer + min(index_in_layer, 0);
		} else {
			spiral_pos.x = spiral_layer + min(index_in_layer, 0) + 1;
			spiral_pos.y = -spiral_layer + lmax - 1;
		}
		if (!params.landscape) {
			spiral_pos = spiral_pos.yx;
		}
	} else { // not spiral
		spiral_pos = params.landscape ? ivec2(inv_index / params.half_size.y, inv_index % params.half_size.y) : params.half_size - 1 - pos;
	}
	spiral_pos = params.half_size - 1 - spiral_pos;

	const vec2 spiral_pixel_pos = spiral_pos + 0.5;
	//	const vec2 spiral_pixel_center_pos = min(vec2(0.0), spiral_pixel_pos - params.center);
	const vec2 spiral_pixel_center_pos = spiral_pixel_pos - params.center;

	//	const float pixel_center_distance = length(spiral_pixel_center_pos);
	// const float pixel_center_distance_sq = dot(spiral_pixel_center_pos, spiral_pixel_center_pos);

	const float pixel_center_distance = distance(params.center, spiral_pixel_pos);

	if (pixel_center_distance <= params.minimum_distance) {
		imageStore(refraction_image, pos, vec4(spiral_pos, vec2(0.0)));
		return;
	}

	const vec2 spiral_uv = spiral_pixel_pos / vec2(params.full_size);

	float blue_center_distance = params.minimum_distance + params.linear_factor * sin(params.edge_factor * (pixel_center_distance - params.minimum_distance)) / params.edge_factor;

	// const vec2 blue_center_pos = spiral_pixel_center_pos * blue_center_distance / pixel_center_distance;
	const vec2 blue_center_pos = normalize(spiral_pixel_center_pos) * blue_center_distance;

	const vec2 blue_pos = blue_center_pos + params.center;

	const float sample_pixel_range = pixel_center_distance - blue_center_distance;
	if (sample_pixel_range <= EPSILON) {
		imageStore(refraction_image, pos, vec4(spiral_pos, vec2(0.0)));
		return;
	}
	// if (sample_pixel_range <= EPSILON && pixel_center_distance <= params.minimum_distance) {
	// 	imageStore(refraction_image, pos, vec4(spiral_pos, vec2(4.5)));
	// 	return;
	// }
	// if (sample_pixel_range <= EPSILON) {
	// 	if (pixel_center_distance > 46) {
	// 		imageStore(refraction_image, pos, vec4(spiral_pos, vec2(5.5)));
	// 		return;
	// 	}
	// 	imageStore(refraction_image, pos, vec4(spiral_pos, vec2(3.5)));
	// 	return;
	// }
	// if (pixel_center_distance <= params.minimum_distance) {
	// 	imageStore(refraction_image, pos, vec4(spiral_pos, vec2(2.5)));
	// 	return;
	// }
	const float samples = params.max_samples;

	const vec2 blue_uv = blue_pos / vec2(params.full_size);

	const vec2 sample_uv_range = blue_uv - spiral_uv;
	//const uint samples = clamp(sample_pixel_range, 3, );
	//	const vec2 base_uv_delta = EPSILON3;
	imageStore(refraction_image, pos, vec4(spiral_pos, sample_uv_range));
#endif

#ifdef MODE_CA_PROCESS
	const ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.half_size))) { //too large, do nothing
		return;
	}

	const vec4 refraction_data = texelFetch(refraction_texture, pos, 0);

	// const ivec2 spiral_pos1 = ivec2(refraction_data.xy);
	const ivec2 spiral_pos1 = ivec2(refraction_data.xy);

	const vec2 sample_uv_range = refraction_data.zw;
	// if (all(lessThanEqual(sample_uv_range, vec2(0.0)))) {
	// 	return;
	// }
	// if (int(sample_uv_range.x) == 5) {
	// 	imageStore(final_image, spiral_pos1, vec4(0.0, 0.0, 1.0, 1.0));
	// 	return;
	// }
	// if (int(sample_uv_range.x) == 4) {
	// 	imageStore(final_image, spiral_pos1, vec4(1.0, 1.0, 0.0, 1.0));
	// 	return;
	// }
	// if (int(sample_uv_range.x) == 3) {
	// 	imageStore(final_image, spiral_pos1, vec4(0.0, 1.0, 0.0, 1.0));
	// 	return;
	// }
	// if (int(sample_uv_range.x) == 2) {
	// 	imageStore(final_image, spiral_pos1, vec4(1.0, 0.0, 0.0, 1.0));
	// 	return;
	// }

	const ivec2 spiral_pos2 = params.full_size - 1 - spiral_pos1;
	const ivec2 spiral_pos3 = ivec2(spiral_pos2.x, spiral_pos1.y);
	const ivec2 spiral_pos4 = ivec2(spiral_pos1.x, spiral_pos2.y);

	const vec2 spiral_pixel_pos1 = spiral_pos1 + 0.5;
	const vec2 spiral_pixel_pos2 = spiral_pos2 + 0.5;
	const vec2 spiral_pixel_pos3 = spiral_pos3 + 0.5;
	const vec2 spiral_pixel_pos4 = spiral_pos4 + 0.5;

	const vec2 spiral_uv1 = spiral_pixel_pos1 / params.full_size;
	const vec2 spiral_uv2 = spiral_pixel_pos2 / params.full_size;
	const vec2 spiral_uv3 = spiral_pixel_pos3 / params.full_size;
	const vec2 spiral_uv4 = spiral_pixel_pos4 / params.full_size;

	// const mat2x4 spiral_uv = mat2x4(spiral_uv1,
	// 		spiral_uv2,
	// 		spiral_uv3,
	// 		spiral_uv4);
	//const uint samples = uint(floor(refraction_data.z));
	//	const uint samples = uint(max(sample_uv_range / (params.pixel_diagonal), 3));
	//const uint samples = uint(fract(refraction_data.x) * 1024);
	//const float samples_init = params.max_samples;
	const float samples = params.max_samples;
	// const uint samples = 100;

	//	uint samples_init = params.max_samples;
	vec2 base_uv_delta = sample_uv_range / samples;
	//	float uv_delta_length = length(base_uv_delta);
	// if (length(base_uv_delta) < params.min_uv_delta) {
	// 	samples_init = max(3, floor(length(sample_uv_range) / params.min_uv_delta));
	// 	base_uv_delta = sample_uv_range / samples_init;
	// }
	// const float samples = samples_init;
	//	const uint samples = clamp(samples_init, 3, uint(length(sample_uv_range) * params.pixel_third * 3));
	const vec2 uv_delta1 = base_uv_delta;
	const vec2 uv_delta2 = -base_uv_delta;
	const vec2 uv_delta3 = vec2(uv_delta2.x, uv_delta1.y);
	const vec2 uv_delta4 = vec2(uv_delta1.x, uv_delta2.y);

	const float spectrum_delta = 0.8 / samples;

	// const float rando = 0.5 + (nrand(vec2(pos)) - 0.5) * params.jitter_amount;
	const float rando = 0.5 + (hash12n(vec2(pos) + params.jitter_seed) - 0.5) * params.jitter_amount;

	vec2 base_sample_uv1 = spiral_uv1 + uv_delta1 * rando;
	vec2 base_sample_uv2 = spiral_uv2 + uv_delta2 * rando;
	vec2 base_sample_uv3 = spiral_uv3 + uv_delta3 * rando;
	vec2 base_sample_uv4 = spiral_uv4 + uv_delta4 * rando;

	float base_spectrum_uv_x = 0.1 + spectrum_delta * rando;
	const float base_spectrum_uv_y = 0.5;
	vec3 filter_sum = vec3(0.0);
	vec3 sum1 = vec3(0.0);
	vec3 sum2 = vec3(0.0);
	vec3 sum3 = vec3(0.0);
	vec3 sum4 = vec3(0.0);

	for (uint i = 0; i < samples; ++i) {
		const vec3 spectrum_filter = texture(spectrum_texture, vec2(base_spectrum_uv_x, base_spectrum_uv_y)).rgb;

		sum1 += texture(copy_texture, base_sample_uv1).rgb * spectrum_filter;
		sum2 += texture(copy_texture, base_sample_uv2).rgb * spectrum_filter;
		sum3 += texture(copy_texture, base_sample_uv3).rgb * spectrum_filter;
		sum4 += texture(copy_texture, base_sample_uv4).rgb * spectrum_filter;

		filter_sum += spectrum_filter;

		base_spectrum_uv_x += spectrum_delta;
		base_sample_uv1 += uv_delta1;
		base_sample_uv2 += uv_delta2;
		base_sample_uv3 += uv_delta3;
		base_sample_uv4 += uv_delta4;
	}

	imageStore(final_image, spiral_pos1, vec4(sum1 / filter_sum, 1.0));
	imageStore(final_image, spiral_pos2, vec4(sum2 / filter_sum, 1.0));
	imageStore(final_image, spiral_pos3, vec4(sum3 / filter_sum, 1.0));
	imageStore(final_image, spiral_pos4, vec4(sum4 / filter_sum, 1.0));

#endif
}