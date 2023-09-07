#[compute]

#version 450

#VERSION_DEFINES

#define BLOCK_SIZE 8

layout(local_size_x = BLOCK_SIZE, local_size_y = BLOCK_SIZE, local_size_z = 1) in;

#ifdef MODE_CA_NORMAL
layout(set = 1, binding = 0) uniform sampler2D color_texture;
layout(rgba16f, set = 0, binding = 0) uniform restrict image2D ca_image;
#endif

#ifdef MODE_CA_COMPOSITE
layout(rgba16f, set = 0, binding = 0) uniform restrict image2D color_image;
layout(set = 1, binding = 0) uniform sampler2D source_ca;
#endif

#include "chromatic_aberration_inc.glsl"

//vec2 channel_sample(vec2 half_pos, float pixel_distance, float image_distance_sample) {
//	float lens_distance = pixel_distance * (params.image_distance_blue + params.focal_length) / params.focal_length;
//	//float incidence = atan(uv_distance / lens_distance);
//	float incidence = atan(pixel_distance / params.focal_length);
//	//float focal_offset_red = params.focal_length * pixel_distance / lens_distance;
//	//	float refracted = atan((pixel_distance + focal_offset) / params.focal_length);
//	float refracted = atan(1.5 * pixel_distance / params.focal_length);
//	float refracted_no_atan = (pixel_distance + lens_distance) / params.image_distance_blue;
//	float sample_distance = pixel_distance - (params.image_distance_blue - image_distance_sample) * refracted_no_atan;
//	return 0.5 + normalize(half_pos) * sample_distance / vec2(params.size);
//}

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

	if (any(greaterThan(pos, params.size))) { //too large, do nothing
		return;
	}

	vec2 pixel_size = 1.0 / vec2(params.size);
	vec2 uv = vec2(pos) / vec2(params.size);
#ifdef MODE_CA_NORMAL
	uv += pixel_size * 0.5; //half pixel to read centers

	//	vec2 half_vec_uv = uv - 0.5;
	//	float center_length = length(half_vec);
	//ivec2 offset = ivec2(round(2.0 * params.ca_axial * (vec2(pos) - vec2(params.size) / 2.0) / vec2(params.size)));

	// ivec2 offset_pos_red = pos - offset;
	// ivec2 offset_pos_blue = pos + offset;
	// vec2 offset = (uv - 0.5) * params.ca_axial;
	// ivec2 half_pos = vec2(float(pos.x) - float(params.size.x) * 0.5, float(pos.y) - float(params.size.y) * 0.5);
	// vec2 half_pos = vec2(pos) - vec2(params.size) * 0.5;
	vec2 half_uv = uv - 0.5;
	vec2 half_pos = half_uv * vec2(params.size);

	float uv_distance = length(half_uv);
	float pixel_distance = length(half_pos);
	float center_ratio = 2.0 * pixel_distance / length(params.size);
	//float view_angle = params.diagonal_fov * center_ratio;

	float sensor_distance = params.sensor_half_diagonal * center_ratio;

	float inner_center_line = params.lens_center_line * center_ratio;
	// float view_angle = atan(sensor_distance / params.lens_distance);
	float view_angle = atan(sensor_distance / params.focal_length);
	// float sensor_angle = atan(params.lens_distance / (3.0 * params.sensor_half_diagonal));
	// float circle_angle = M_HALF_PI - sensor_angle;
	float far_angle = M_HALF_PI - view_angle;
	float incident_angle = asin(sin(far_angle) * inner_center_line / params.curvature_radius);
	// float normal_angle = M_HALF_PI - params.diagonal * 0.5 - radius_angle;
	float normal_angle = incident_angle - params.diagonal_half_fov;

	float refract_chord = params.curvature_radius * cos(normal_angle);

	//float incident_angle = view_angle + radius_angle;
	float refracted_angle_red = asin(params.refract_index_red * sin(incident_angle));
	float refracted_angle_green = asin(params.refract_index_green * sin(incident_angle));
	float refracted_angle_blue = asin((params.refract_index_green + 0.01) * sin(incident_angle));

	// vec2 refracted_angle = vec2(asin(nr * sin(I)), asin(ng * sin(I)));
	//	float refracted_angle_blue = asin(nb * sin(I));

	float y = params.lens_distance - sqrt(params.curvature_radius * params.curvature_radius - refract_chord * refract_chord) + params.apothem;
	float sample_offset_red_mm = y * tan(refracted_angle_red - normal_angle) - refract_chord;
	float sample_offset_green_mm = y * tan(refracted_angle_green - normal_angle) - refract_chord;
	float sample_offset_blue_mm = y * tan(refracted_angle_blue - normal_angle) - refract_chord;

	float sample_offset_red_pixels = sample_offset_red_mm * 0.5 * length(params.size) / params.sensor_half_diagonal;
	float sample_offset_green_pixels = sample_offset_green_mm * 0.5 * length(params.size) / params.sensor_half_diagonal;
	float sample_offset_blue_pixels = sample_offset_blue_mm * 0.5 * length(params.size) / params.sensor_half_diagonal;

	//	vec2 sample_half_pos_red = half_pos - normalize(half_pos) * sample_offset_red;
	//	vec2 sample_half_pos_green = half_pos - normalize(half_pos) * sample_offset_green;
	vec2 sample_half_pos_red = normalize(half_pos) * sample_offset_red_pixels;
	vec2 sample_half_pos_green = normalize(half_pos) * sample_offset_green_pixels;
	vec2 sample_half_pos_blue = normalize(half_pos) * sample_offset_blue_pixels;

	vec2 sample_uv_red = 0.5 + (sample_half_pos_red / vec2(params.size));
	vec2 sample_uv_green = 0.5 + (sample_half_pos_green / vec2(params.size));
	vec2 sample_uv_blue = 0.5 + (sample_half_pos_blue / vec2(params.size));

	//	float sample_offset_green = (y * tan(refracted_angle_green) - refract_chord) * length(params.size) / params.sensor_diagonal;
	//	float inner_apothem = params.curvature_radius * sin(normal_angle);
	//	vec2 normal = vec2(refract_chord, -inner_apothem)
	//			vec2 incident = vec2(-sensor_distance, -params.lens_distance);
	//	vec2 refracted = vec2(refract_chord, -inner_apothem);

	//	float uv_distance = length(half_pos);
	//	float lens_distance = uv_distance * (params.image_distance_blue + params.lens_distance) / params.lens_distance;
	//	//float incidence = atan(uv_distance / lens_distance);
	//	float focal_offset_red = params.lens_distance * uv_distance / lens_distance;
	//	//	float refracted = atan((uv_distance + focal_offset) / params.lens_distance);
	//	float refracted_no_atan = (uv_distance + lens_distance) / params.image_distance_blue;
	//	float sample_distance = uv_distance - (params.image_distance_blue - image_distance_red) * refracted_no_atan;
	//	vec2 sample_pos = normalize(uv_distance) * sample_distance;

	// vec2 red_uv = channel_sample(half_pos, pixel_distance, params.image_distance_red);
	// vec2 green_uv = channel_sample(half_pos, pixel_distance, params.image_distance_green);

	//	if (half_vec.y > 0) {
	//	ivec2 new_pos = ivec2(pos.x, int(float(pos.y) - float(params.size.y) * 0.5));
	//	new_pos = pos;
	//	new_pos = ivec2(pos.x, pos.y + half_vec.y);
	//	vec2 new_half_vec = vec2(new_pos) - vec2(params.size) * 0.5;
	//new_half_vec = -half_vec;
	//new_half_vec = half_vec;

	//vec2 new_offset = new_half_vec * params.ca_axial;
	//new_offset = vec2(-new_offset.x, new_offset.y);
	// vec2 offset_uv_red = uv - offset;
	// vec2 offset_uv_green = uv - 0.5 * offset;
	//	} else {
	//		offset_pos_red = pos - ivec2(offset);
	//		offset_pos_green = pos - ivec2(0.5 * offset);
	//	}
	//	offset_pos_red.x = pos.x - 2 * offset.x;
	//	offset_pos_red.y = pos.x - 2 * offset.y;
	//	offset_pos_green.x = pos.y - offset.x;
	//	offset_pos_green.y = pos.y - offset.y;
	//} else {
	//	offset_pos_red = pos - 2 * offset;
	//	offset_pos_green = pos - offset;

	//offset_pos_red.x = pos.x + 2 * offset.x;
	//offset_pos_red.y = pos.x + 2 * offset.y;
	//offset_pos_green.x = pos.y + offset.x;
	//offset_pos_green.y = pos.y + offset.y;
	//	}
	//	offset_pos_red = pos - 2 * offset;
	//	offset_pos_green = pos - offset;

	// offset_pos_red = pos;
	// offset_pos_blue = pos;
	vec4 color = texture(color_texture, uv);

	//float color_ca_red = texture(color_texture, offset_uv_red).r;
	//float color_ca_green = texture(color_texture, offset_uv_green).g;
	//float color_ca_red = texture(color_texture, red_uv).r;
	//float color_ca_green = texture(color_texture, green_uv).g;
	//color.r = color_ca_red;
	//color.g = color_ca_green;
	float color_ca_red = texture(color_texture, sample_uv_red).r;
	float color_ca_green = texture(color_texture, sample_uv_green).g;
	float color_ca_blue = texture(color_texture, sample_uv_blue).g;

	color.r = color_ca_red;
	color.g = color_ca_green;
	//color.r = abs(float(offset.x)) * 2.0 / float(params.size.x);
	//color.g = abs(float(offset.y)) * 2.0 / float(params.size.y);
	//color.r = float(offset_pos_red.x) / float(params.size.x);
	//color.g = float(offset_pos_red.y) / float(params.size.y);
	// color.b = 0;
	// color.a = 1;

	// if (red_uv.x == 0.0 && red_uv.y == 0.0 && green_uv.x == 0.0 && green_uv.y == 0.0) {
	//imageStore(ca_image, pos, vec4(1, 0, 0, 1));
	//} else {
	imageStore(ca_image, pos, color);
	//}
#endif
#ifdef MODE_CA_COMPOSITE
	vec4 color = texture(source_ca, uv);
	imageStore(color_image, pos, color);

#endif
}