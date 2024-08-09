#[compute]

#version 450

#VERSION_DEFINES

#define BLOCK_SIZE 8

//layout(local_size_x = BLOCK_SIZE, local_size_y = BLOCK_SIZE, local_size_z = 1) in;
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#if defined(MODE_CA_PROCESS_TWO_TONE) || defined(MODE_CA_PROCESS_THREE_TONE) || defined(MODE_CA_PROCESS_SPECTRUM)
// layout(rgba16f, set = 0, binding = 0) uniform restrict writeonly image2D ca_image;
layout(set = 0, binding = 0) uniform sampler2D copy_texture;
// layout(set = 1, binding = 0) uniform sampler2D refraction_texture;
// layout(set = 2, binding = 0) uniform sampler2D color_texture;
layout(rgba16f, set = 1, binding = 0) uniform restrict writeonly image2D ca_image;
//layout(rgba16f, set = 4, binding = 0) uniform restrict writeonly image2D final_image;
#endif

#ifdef MODE_CA_COMPOSITE
layout(rgba16f, set = 0, binding = 0) uniform restrict writeonly image2D final_image;
layout(set = 1, binding = 0) uniform sampler2D ca_texture;
#endif

#include "chromatic_aberration_inc.glsl"

#if defined(MODE_CA_PROCESS_TWO_TONE) || defined(MODE_CA_PROCESS_THREE_TONE) || defined(MODE_CA_PROCESS_SPECTRUM)

// float triangle_wave(const float period, const float t) {
// 	return 2.0 * abs(t / period - floor(t / period + 0.5));
// }

float nrand(const vec2 n) {
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}
// highp float nrand(vec2 co) {
// 	highp float a = 12.9898;
// 	highp float b = 78.233;
// 	highp float c = 43758.5453;
// 	highp float dt = dot(co.xy, vec2(a, b));
// 	highp float sn = mod(dt, 3.14);
// 	return fract(sin(sn) * c);
// }
#endif

void main() {
	// #ifdef MODE_CA_SPECTRUM
	// 	const ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	// 	if (any(greaterThanEqual(pos, params.size))) { //too large, do nothing
	// 		return;
	// 	}

	// 	if (pos.x == 0) {
	// 		imageStore(spectrum_image, pos, vec4(0.0, 0.0, 0.0, 1.0));
	// 	} else if (pos.x == 1) {
	// 		imageStore(spectrum_image, pos, vec4(1.0, 0.0, 0.0, 1.0));
	// 	} else if (pos.x == 2) {
	// 		imageStore(spectrum_image, pos, vec4(0.0, 1.0, 0.0, 1.0));
	// 	} else if (pos.x == 3) {
	// 		imageStore(spectrum_image, pos, vec4(0.0, 0.0, 1.0, 1.0));
	// 	} else if (pos.x == 4) {
	// 		imageStore(spectrum_image, pos, vec4(0.0, 0.0, 0.0, 1.0));
	// 	}

	// 	// if (pos.x == 0) {
	// 	// 	imageStore(spectrum_image, pos, vec4(1.0, params.desaturation, params.desaturation, 1.0));
	// 	// } else if (pos.x == 1) {
	// 	// 	imageStore(spectrum_image, pos, vec4(params.desaturation, 1.0, params.desaturation, 1.0));
	// 	// } else if (pos.x == 2) {
	// 	// 	imageStore(spectrum_image, pos, vec4(params.desaturation, params.desaturation, 1.0, 1.0));
	// 	// }

	// #endif

#if defined(MODE_CA_PROCESS_TWO_TONE) || defined(MODE_CA_PROCESS_THREE_TONE) || defined(MODE_CA_PROCESS_SPECTRUM)
	const ivec2 pos1 = ivec2(gl_GlobalInvocationID.xy);
	if (any(greaterThanEqual(pos1, params.half_size))) { //too large, do nothing
		return;
	}
	const ivec2 pos2 = params.full_size - 1 - pos1;
	const ivec2 pos3 = ivec2(pos2.x, pos1.y);
	const ivec2 pos4 = ivec2(pos1.x, pos2.y);
	const vec2 pixel_pos = pos1 + 0.5;
	const vec2 pixel_center_pos = pixel_pos - params.center;
	const float pixel_center_distance = distance(params.center, pixel_pos);
	const vec2 uv1 = pixel_pos / vec2(params.full_size);
	const vec2 uv2 = vec2(1.0) - uv1;
	const vec2 uv3 = vec2(uv2.x, uv1.y);
	const vec2 uv4 = vec2(uv1.x, uv2.y);
	if (pixel_center_distance <= params.minimum_distance) {
		imageStore(ca_image, pos1, texture(copy_texture, uv1));
		imageStore(ca_image, pos2, texture(copy_texture, uv2));
		imageStore(ca_image, pos3, texture(copy_texture, uv3));
		imageStore(ca_image, pos4, texture(copy_texture, uv4));
		return;
	}
	float blue_center_distance = params.minimum_distance + sin(params.edge_factor * (pixel_center_distance - params.minimum_distance)) / params.edge_factor;
	const vec2 blue_center_pos = normalize(pixel_center_pos) * blue_center_distance;
	const vec2 blue_pos = blue_center_pos + params.center;
	const float sample_pixel_range = pixel_center_distance - blue_center_distance;
	const vec2 pixel_range = abs(blue_pos - pixel_pos);
	if (all(lessThanEqual(pixel_range, vec2(EPSILON)))) {
		imageStore(ca_image, pos1, texture(copy_texture, uv1));
		imageStore(ca_image, pos2, texture(copy_texture, uv2));
		imageStore(ca_image, pos3, texture(copy_texture, uv3));
		imageStore(ca_image, pos4, texture(copy_texture, uv4));
		return;
	}
	float samples = params.max_samples;
	// const vec2 pixel_delta = pixel_range / samples;

	// if (params.sample_mode == SAMPLE_MODE_SPECTRUM) {
	// 	if (all(lessThanEqual(pixel_delta, vec2(EPSILON)))) {
	// 		samples = ceil(0.25 * sample_pixel_range / EPSILON) * 4;
	// 	}
	// }

	const vec2 blue_uv = blue_pos / vec2(params.full_size);
	const vec2 sample_uv_range = blue_uv - uv1;

	const vec2 uv_delta1 = sample_uv_range / samples;
	const vec2 uv_delta2 = -uv_delta1;
	const vec2 uv_delta3 = vec2(uv_delta2.x, uv_delta1.y);
	const vec2 uv_delta4 = vec2(uv_delta1.x, uv_delta2.y);
#endif

#ifdef MODE_CA_PROCESS_TWO_TONE
	vec3 sum1 = texture(copy_texture, uv1).rgb * filter1 +
			texture(copy_texture, uv1 + uv_delta1).rgb * filter2;
	vec3 sum2 = texture(copy_texture, uv2).rgb * filter1 +
			texture(copy_texture, uv2 + uv_delta2).rgb * filter2;
	vec3 sum3 = texture(copy_texture, uv3).rgb * filter1 +
			texture(copy_texture, uv3 + uv_delta3).rgb * filter2;
	vec3 sum4 = texture(copy_texture, uv4).rgb * filter1 +
			texture(copy_texture, uv4 + uv_delta4).rgb * filter2;
#endif

#ifdef MODE_CA_PROCESS_THREE_TONE
	vec3 sample11 = texture(copy_texture, uv1).rgb;
	vec3 sample12 = texture(copy_texture, uv1 + uv_delta1).rgb;
	vec3 sample13 = texture(copy_texture, uv1 + 2 * uv_delta1).rgb;
	vec3 sample21 = texture(copy_texture, uv2).rgb;
	vec3 sample22 = texture(copy_texture, uv2 + uv_delta2).rgb;
	vec3 sample23 = texture(copy_texture, uv2 + 2 * uv_delta2).rgb;
	vec3 sample31 = texture(copy_texture, uv3).rgb;
	vec3 sample32 = texture(copy_texture, uv3 + uv_delta3).rgb;
	vec3 sample33 = texture(copy_texture, uv3 + 2 * uv_delta3).rgb;
	vec3 sample41 = texture(copy_texture, uv4).rgb;
	vec3 sample42 = texture(copy_texture, uv4 + uv_delta4).rgb;
	vec3 sample43 = texture(copy_texture, uv4 + 2 * uv_delta4).rgb;
	float desat = params.desaturation / 3.0;
	float inv_desat = (3.0 - 2.0 * params.desaturation) / 3.0;

	// vec3 sum1 =  vec3(texture(copy_texture, uv1).r, texture(copy_texture, uv1 + uv_delta1).g, texture(copy_texture, uv1 + 2 * uv_delta1).b);
	// vec3 sum2 =  vec3(texture(copy_texture, uv2).r, texture(copy_texture, uv2 + uv_delta2).g, texture(copy_texture, uv2 + 2 * uv_delta2).b);
	// vec3 sum3 =  vec3(texture(copy_texture, uv3).r, texture(copy_texture, uv3 + uv_delta3).g, texture(copy_texture, uv3 + 2 * uv_delta3).b);
	// vec3 sum4 =  vec3(texture(copy_texture, uv4).r, texture(copy_texture, uv4 + uv_delta4).g, texture(copy_texture, uv4 + 2 * uv_delta4).b);
	vec3 sum1 = vec3(sample11.r * inv_desat + (sample12.r + sample13.r) * desat, sample12.g * inv_desat + (sample11.g + sample13.g) * desat, sample13.b * inv_desat + (sample11.b + sample12.b) * desat);
	vec3 sum2 = vec3(sample21.r * inv_desat + (sample22.r + sample23.r) * desat, sample22.g * inv_desat + (sample21.g + sample23.g) * desat, sample23.b * inv_desat + (sample21.b + sample22.b) * desat);
	vec3 sum3 = vec3(sample31.r * inv_desat + (sample32.r + sample33.r) * desat, sample32.g * inv_desat + (sample31.g + sample33.g) * desat, sample33.b * inv_desat + (sample31.b + sample32.b) * desat);
	vec3 sum4 = vec3(sample41.r * inv_desat + (sample42.r + sample43.r) * desat, sample42.g * inv_desat + (sample41.g + sample43.g) * desat, sample43.b * inv_desat + (sample41.b + sample42.b) * desat);
#endif

#if defined(MODE_CA_PROCESS_TWO_TONE) || defined(MODE_CA_PROCESS_THREE_TONE)
	imageStore(ca_image, pos1, vec4(sum1, 1.0));
	imageStore(ca_image, pos2, vec4(sum2, 1.0));
	imageStore(ca_image, pos3, vec4(sum3, 1.0));
	imageStore(ca_image, pos4, vec4(sum4, 1.0));
#endif

#ifdef MODE_CA_PROCESS_SPECTRUM
	const float spectrum_delta = 0.8 / samples;
	float rando;
	if (params.jitter) {
		// rando = nrand(vec2(pos1) + params.jitter_seed);
		//	rando = fract(sin(dot(vec2(pos1) + params.jitter_seed, vec2(12.9898, 78.233))) * 43758.5453);
		rando = hash12n(vec2(pos1) + params.jitter_seed);
	} else {
		rando = 0.5;
	}

	vec2 base_sample_uv1 = uv1 + uv_delta1 * rando;
	vec2 base_sample_uv2 = uv2 + uv_delta2 * rando;
	vec2 base_sample_uv3 = uv3 + uv_delta3 * rando;
	vec2 base_sample_uv4 = uv4 + uv_delta4 * rando;

	float base_spectrum_uv_x = 0.1 + spectrum_delta * rando;

	const float base_spectrum_uv_y = 0.5;
	//vec3 filter_sum = vec3(0.0);
	vec3 sum1 = vec3(0.0);
	vec3 sum2 = vec3(0.0);
	vec3 sum3 = vec3(0.0);
	vec3 sum4 = vec3(0.0);

	for (uint i = 0; i < samples; ++i) {
		float desat = 1.0 - params.desaturation;

		vec3 spectrum_filter = max(vec3(0.0), 1.0 - abs(vec3(5.0 * base_spectrum_uv_x) - vec3(1.5, 2.5, 3.5)));
		const vec3 inv_spec = params.desaturation * (1.0 - spectrum_filter);
		spectrum_filter += inv_spec;
		sum1 += texture(copy_texture, base_sample_uv1).rgb * spectrum_filter;
		sum2 += texture(copy_texture, base_sample_uv2).rgb * spectrum_filter;
		sum3 += texture(copy_texture, base_sample_uv3).rgb * spectrum_filter;
		sum4 += texture(copy_texture, base_sample_uv4).rgb * spectrum_filter;

		//filter_sum += spectrum_filter;

		base_spectrum_uv_x += spectrum_delta;
		base_sample_uv1 += uv_delta1;
		base_sample_uv2 += uv_delta2;
		base_sample_uv3 += uv_delta3;
		base_sample_uv4 += uv_delta4;
	}
	imageStore(ca_image, pos1, vec4(sum1 / (samples / 4.0), 1.0));
	imageStore(ca_image, pos2, vec4(sum2 / (samples / 4.0), 1.0));
	imageStore(ca_image, pos3, vec4(sum3 / (samples / 4.0), 1.0));
	imageStore(ca_image, pos4, vec4(sum4 / (samples / 4.0), 1.0));
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