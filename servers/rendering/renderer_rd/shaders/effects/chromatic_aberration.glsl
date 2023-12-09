#[compute]

#version 450

#VERSION_DEFINES

#define BLOCK_SIZE 8

layout(local_size_x = BLOCK_SIZE, local_size_y = BLOCK_SIZE, local_size_z = 1) in;

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
layout(rgba16f, set = 3, binding = 0) uniform restrict writeonly image2D ca_image;
layout(rgba16f, set = 4, binding = 0) uniform restrict writeonly image2D final_image;

#endif

#ifdef MODE_CA_COMPOSITE
layout(rgba16f, set = 0, binding = 0) uniform restrict writeonly image2D final_image;
layout(set = 1, binding = 0) uniform sampler2D ca_texture;
#endif

#include "chromatic_aberration_inc.glsl"

#ifdef MODE_CA_PROCESS

float triangle_wave(float period, float t) {
	return 2.0 * abs(t / period - floor(t / period + 0.5));
}

float nrand(vec2 n) {
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

#endif

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

	if (any(greaterThan(pos, params.size))) { //too large, do nothing
		return;
	}

#ifdef MODE_CA_SPECTRUM

	if (pos.x == 0) {
		imageStore(spectrum_image, pos, vec4(0.0, 0.0, 0.0, 1.0));
	} else if (pos.x == 1) {
		imageStore(spectrum_image, pos, vec4(1.0, 0.0, 0.0, 1.0));
	} else if (pos.x == 2) {
		imageStore(spectrum_image, pos, vec4(0.0, 1.0, 0.0, 1.0));
	} else if (pos.x == 3) {
		imageStore(spectrum_image, pos, vec4(0.0, 0.0, 1.0, 1.0));
	} else if (pos.x == 4) {
		imageStore(spectrum_image, pos, vec4(0.0, 0.0, 0.0, 1.0));
	}

	// if (pos.x == 0) {
	// 	imageStore(spectrum_image, pos, vec4(1.0, 0.0, 0.0, 1.0));
	// } else if (pos.x == 1) {
	// 	imageStore(spectrum_image, pos, vec4(0.0, 1.0, 0.0, 1.0));
	// } else if (pos.x == 2) {
	// 	imageStore(spectrum_image, pos, vec4(0.0, 0.0, 1.0, 1.0));
	// }

#endif

	// #ifdef MODE_CA_REFRACTION_LINE

	// 	float dist_factor = M_HALF_PI / params.size.x;
	// 	float blue_center_distance = sin(pos.x * dist_factor) / dist_factor;
	// 	float sample_pixel_range = pos.x - blue_center_distance;
	// 	float samples = max(3.0, ceil(sample_pixel_range * params.quality));
	// 	float delta_pixel = sample_pixel_range / (samples - 1.0);

	// 	//	imageStore(refraction_image, pos, vec4(1.0, 0.0, 0.0, 1.0));
	// 	imageStore(refraction_image, pos, vec4(blue_center_distance, sample_pixel_range, delta_pixel, samples));

	// #endif

#ifdef MODE_CA_REFRACTION_BOX
	vec2 pixel_pos = pos + 0.5;
	float pixel_center_distance = length(pixel_pos);
	//	if (pixel_center_distance < params.center_offset) {
	//		return;
	//	}
	vec2 uv = pixel_pos / vec2(params.size);

	//	float blue_center_distance = params.center_offset + sin((pixel_center_distance - params.center_offset) / params.intensity) * params.intensity;
	//	float blue_center_distance = sin((pixel_center_distance)*params.intensity) / params.intensity;
	float blue_center_distance = params.center_offset + params.curve * sin((pixel_center_distance - params.center_offset) * params.intensity) / params.intensity;

	float sample_pixel_range = pixel_center_distance - blue_center_distance;
	// blue_center_distance = 0.0;
	float samples = 1.0 + ceil(sample_pixel_range * params.quality);
	//float samples = ceil(10.0 * params.quality);

	float delta_pixel = sample_pixel_range / samples;
	vec2 blue_uv = uv * blue_center_distance / pixel_center_distance;

	imageStore(refraction_image, pos, vec4(blue_uv, sample_pixel_range, samples));

//	if (pixel_center_distance < params.center_offset) {
//		imageStore(refraction_image, pos, vec4(uv, 0.0, 0.0));
//	}
#endif

#ifdef MODE_CA_PROCESS
	vec2 uv = params.half_pixel_size + vec2(pos) / vec2(params.size);
	//	vec2 pixel_pos = pos + 0.5;
	// vec2 half_pos = pixel_pos - 0.5 * params.size;
	vec2 half_uv = uv - 0.5;
	// vec2 dir_uv = normalize(half_uv);
	// vec2 dir_pos = normalize(half_pos);
	//	float pixel_center_distance = length(half_pos);
	// float lookup = pixel_center_distance / params.half_diagonal;
	//	vec4 refraction_data = texture(refraction_texture, vec2(lookup, 0.0));
	vec4 refraction_data = texture(refraction_texture, abs(half_uv * 2.0));
	vec2 blue_uv = 0.5 + sign(half_uv) * refraction_data.xy * 0.5;

	float sample_pixel_range = refraction_data.z;
	vec2 pixel_pos = pos + 0.5;
	vec2 half_pos = pixel_pos - params.size * 0.5;
	float pixel_center_distance = length(half_pos);

	if (sample_pixel_range < EPSILON) {
		imageStore(ca_image, pos, texture(copy_texture, uv));
		//	if (pixel_center_distance < params.center_offset) {
		//		imageStore(ca_image, pos, texture(copy_texture, uv));
		//	}
		return;
	}

	float samples = refraction_data.w;
	//vec2 blue_pos = pixel_pos - dir_pos * sample_pixel_range;
	//vec2 blue_uv = vec2(blue_pos) / vec2(params.size);
	// vec2 green_uv = blue_uv + 0.5 * (uv - blue_uv);
	vec2 sample_uv_range = uv - blue_uv;
	vec2 delta_uv = sample_uv_range / samples;

	// vec2 blue_half_pos = dir_pos * blue_center_distance;
	//float rando = nrand(uv + params.jitter_seed) - 0.5;
	//vec2 sample_uv = blue_uv + delta_uv * (0.5 + rando);
	float rando = nrand(uv + params.jitter_seed) - 0.5;
	vec2 sample_uv = uv - 0.5 * delta_uv - delta_uv * rando;
	//float spec_jit = pixel_center_distance - delta_pixel * (0.5 + rando);
	//vec3 filter_sum = vec3(1.0);
	//	float spectrum_delta = 4.0 / (5.0 * samples);
	//	vec2 spectrum_uv = vec2(1.0 / 10.0 + spectrum_delta * (0.5 + rando), 0.0);
	float spectrum_delta = 0.8 / samples;
	vec2 spectrum_uv = vec2(0.1 + spectrum_delta * rando + 0.5 * spectrum_delta, 0.0);

	vec3 filter_sum = texture(spectrum_texture, spectrum_uv).rgb;
	//filter_sum = vec3(texture(spectrum_texture, vec2(0.3, 0.0)).r,
	//		0.0,
	//		texture(spectrum_texture, vec2(0.7, 0.0)).b);
	// filter_sum = vec3(0.0);
	vec3 sum = filter_sum * texture(copy_texture, sample_uv).rgb;

	// vec3 sum = vec3(texture(copy_texture, uv).r,
	// 		0.0,
	// 		texture(copy_texture, blue_uv).b);
	//	sum = vec3(0.0);
	// float spectrum_pos = 0.3;
	// float spec_amount = 0.4 / (samples - 1.0);

	for (float i = 0.0; i < samples - 1.0; ++i) {
		// float jitter_ratio = delta_pixel * (0.5 + rando + i) / sample_pixel_range;
		sample_uv -= delta_uv;
		spectrum_uv.x += spectrum_delta;
		//	vec3 spectrum_filter =
		//			clamp(vec3(jitter_ratio * 4.0 - 2.0,
		//						  2.0 - 4.0 * abs(jitter_ratio - 0.5),
		//						  2.0 - jitter_ratio * 4.0),
		//					0.0, 1.0);
		//	vec3 spectrum_filter = texture(spectrum_texture, vec2(i / samples, 0.0)).rgb;
		vec3 spectrum_filter = texture(spectrum_texture, spectrum_uv).rgb;

		//	spectrum_pos += spec_amount;
		// spectrum_filter /= 255.0;
		// vec2 sample_uv = spec_jit * dir_uv;
		// vec2 sample_uv = blue_uv + delta_uv * i;
		sum += texture(copy_texture, sample_uv).rgb * spectrum_filter;
		filter_sum += spectrum_filter;

		//	spec_jit -= delta_pixel;
	}

	// imageStore(final_image, pos, texture(refraction_texture, vec2(lookup, 0)));
	//imageStore(final_image, pos, texture(spectrum_texture, vec2(0.75, 0)));
	imageStore(ca_image, pos, vec4(sum / filter_sum, 1.0));
	// imageStore(ca_image, pos, texture(refraction_texture, abs(half_uv * 2.0)));

	// imageStore(ca_image, pos, texture(spectrum_texture, vec2(0.01, 0.0)));

	// imageStore(ca_image, pos, texture(refraction_texture, abs(2.0 * half_uv)));

	// imageStore(ca_image, pos, vec4(pos.x, 0.0, 0.0, 1.0));

//imageStore(final_image, pos, refraction_data);

//	vec4 color = texture(refraction_texture, uv);
//	imageStore(final_image, pos, color);
#endif

#ifdef MODE_CA_COMPOSITE
	vec2 uv = params.half_pixel_size + vec2(pos) / vec2(params.size);

	vec2 half_pos = pos + 0.5 - params.half_diagonal;
	float pixel_center_distance = length(half_pos);

	//	if (sample_pixel_range < EPSILON || pixel_center_distance < params.center_offset) {
	//		return;
	//	}
	//	vec4 new_texture = texture(ca_texture, uv);
	//	if (new_texture.a == 1.0) {
	//		imageStore(final_image, pos, vec4(texture(ca_texture, uv)));
	//	}
	imageStore(final_image, pos, vec4(texture(ca_texture, uv)));

#endif
}