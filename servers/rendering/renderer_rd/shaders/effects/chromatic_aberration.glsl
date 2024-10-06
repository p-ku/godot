#[compute]

#version 450

#VERSION_DEFINES

#define BLOCK_SIZE 8

//layout(local_size_x = BLOCK_SIZE, local_size_y = BLOCK_SIZE, local_size_z = 1) in;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#ifndef MODE_CA_COMPOSITE
// layout(rgba16f, set = 0, binding = 0) uniform restrict writeonly image2D ca_image;
layout(set = 0, binding = 0) uniform sampler2D copy_texture;
// layout(set = 1, binding = 0) uniform sampler2D refraction_texture;
// layout(set = 2, binding = 0) uniform sampler2D color_texture;
layout(rgba16f, set = 1, binding = 0) uniform restrict writeonly image2D ca_image;
//layout(rgba16f, set = 4, binding = 0) uniform restrict writeonly image2D final_image;

// float triangle_wave(const float period, const float t) {
// 	return 2.0 * abs(t / period - floor(t / period + 0.5));
// }

// float nrand(const vec2 n) {
// 	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
// }
// highp float nrand(vec2 co) {
// 	highp float a = 12.9898;
// 	highp float b = 78.233;
// 	highp float c = 43758.5453;
// 	highp float dt = dot(co.xy, vec2(a, b));
// 	highp float sn = mod(dt, 3.14);
// 	return fract(sin(sn) * c);
// }
#endif

#ifdef MODE_CA_COMPOSITE
layout(rgba16f, set = 0, binding = 0) uniform restrict writeonly image2D final_image;
layout(set = 1, binding = 0) uniform sampler2D ca_texture;
#endif

#include "chromatic_aberration_inc.glsl"

void main() {
#ifndef MODE_CA_COMPOSITE
	const ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.size))) { //too large, do nothing
		return;
	}

	const vec2 pixel_pos = pos + 0.5;
	const float pixel_center_distance = distance(params.center, pixel_pos); // distance to center from pos

	const vec2 uv = pixel_pos / vec2(params.size);

	if (pixel_center_distance <= params.minimum_distance) {
		imageStore(ca_image, pos, texture(copy_texture, uv));
		return;
	}
	const vec2 pixel_center_pos = pixel_pos - params.center;

	//float blue_center_distance = params.minimum_distance + sin(params.edge_factor * (pixel_center_distance - params.minimum_distance)) / params.edge_factor;
	//	float blue_center_distance = params.edge_factor * dot(normalize(vec3(pixel_center_pos, 1.0)), vec3(0.0, 0.0, 1.0)) / 3.14159;
	// float blue_center_distance = (1.0 - dot(normalize(vec3(pixel_center_pos, 1.0)), vec3(0.0, 0.0, 1.0))) / params.edge_factor;
	// float new_edge = params.edge_factor * 4.0 / 9.8696;
	float pt = pixel_center_distance - params.minimum_distance;
	//float blue_center_distance = params.minimum_distance + pt * (1.0 - dot(vec2(pt * params.edge_factor, 1.0), vec2(99999999, 0.0000001)));
	float blue_center_distance = params.minimum_distance + pt * cos(params.edge_factor * pt);
	const vec2 blue_center_pos = pixel_center_pos * blue_center_distance / pixel_center_distance;

	//	const vec2 dir = normalize(pixel_center_pos);
	// const vec2 blue_pos = dir * blue_center_distance + params.center;
	const vec2 pixel_range = blue_center_pos - pixel_center_pos;

	float samples = params.max_samples;
	//const vec2 pixel_delta = pixel_range / samples;
	vec2 sample_delta = pixel_range / samples;
	if (all(lessThan(abs(sample_delta), vec2(EPSILON)))) {
		const vec2 abs_range = abs(pixel_range);
		const float large_sample_dim = max(abs_range.x, abs_range.y);
		if (large_sample_dim < EPSILON) {
			imageStore(ca_image, pos, texture(copy_texture, uv));
			return;
		}
		samples = ceil(large_sample_dim / EPSILON);
		sample_delta = pixel_range / samples;
	}

	//const vec2 blue_uv = blue_pos / vec2(params.full_size);
	// const vec2 sample_uv_range = blue_uv - uv;

	//const vec2 sample_uv_range = pixel_range * params.pixel_size;
	// const vec2 uv_delta = sample_uv_range / samples;
	const vec2 uv_delta = sample_delta * params.pixel_size;
	const float spectrum_delta = 6.0 / samples;

#endif

#ifdef MODE_CA_PROCESS_JITTER
	const float rando = hash12n(uv + params.jitter_seed) + 0.0000001;
	if (rando >= 1.0) {
		imageStore(ca_image, pos, vec4(1.0));
		return;
	}
	vec2 sample_uv = uv + uv_delta * rando;
	float spectrum_pos = spectrum_delta * rando - 3.0;
#elif defined(MODE_CA_PROCESS)
	vec2 sample_uv = uv;
	float spectrum_pos = spectrum_delta * 0.5 - 3.0;
#endif

#ifndef MODE_CA_COMPOSITE

	vec3 filter_sum = vec3(0.0);

	vec3 sum = vec3(0.0);
	//for (uint i = 0; i < samples; ++i) {
	while (spectrum_pos < 3.0) {
		const float alpha = min(-abs(spectrum_pos) + 3.0, 1.0);
		const vec3 spectrum_filter = alpha * clamp(vec3(abs(spectrum_pos - 1.0) - 1.0, (-abs(spectrum_pos) + 2.0), abs(spectrum_pos + 1.0) - 1.0), 0.0, 1.0);

		sum += texture(copy_texture, sample_uv).rgb * spectrum_filter;

		filter_sum += spectrum_filter;

		spectrum_pos += spectrum_delta;
		sample_uv += uv_delta;
	}
	imageStore(ca_image, pos, vec4(sum / filter_sum, 1.0));
#endif

#ifdef MODE_CA_COMPOSITE
	const ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos, params.full_size))) { //too large, do nothing
		return;
	}
	const vec2 pixel_pos = pos + 0.5;
	const vec2 uv = pixel_pos / vec2(params.full_size);
	imageStore(final_image, pos, texture(ca_texture, uv));
#endif
}