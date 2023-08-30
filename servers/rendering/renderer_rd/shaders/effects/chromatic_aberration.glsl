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

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);

	if (any(greaterThan(pos, params.size))) { //too large, do nothing
		return;
	}

	vec2 pixel_size = 1.0 / vec2(params.size);
	vec2 uv = vec2(pos) / vec2(params.size);
#ifdef MODE_CA_NORMAL

	//	vec2 half_vec_uv = uv - 0.5;
	//	float center_length = length(half_vec);
	//ivec2 offset = ivec2(round(2.0 * params.ca_axial * (vec2(pos) - vec2(params.size) / 2.0) / vec2(params.size)));

	// ivec2 offset_pos_red = pos - offset;
	// ivec2 offset_pos_blue = pos + offset;
	vec2 offset = (uv - 0.5) * params.ca_axial;

	//	if (half_vec.y > 0) {
	//	ivec2 new_pos = ivec2(pos.x, int(float(pos.y) - float(params.size.y) * 0.5));
	//	new_pos = pos;
	//	new_pos = ivec2(pos.x, pos.y + half_vec.y);
	//	vec2 new_half_vec = vec2(new_pos) - vec2(params.size) * 0.5;
	//new_half_vec = -half_vec;
	//new_half_vec = half_vec;

	//vec2 new_offset = new_half_vec * params.ca_axial;
	//new_offset = vec2(-new_offset.x, new_offset.y);
	vec2 offset_uv_red = uv - offset;
	vec2 offset_uv_green = uv - 0.5 * offset;
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

	float color_ca_red = texture(color_texture, offset_uv_red).r;
	float color_ca_green = texture(color_texture, offset_uv_green).g;

	color.r = color_ca_red;
	color.g = color_ca_green;

	//color.r = abs(float(offset.x)) * 2.0 / float(params.size.x);
	//color.g = abs(float(offset.y)) * 2.0 / float(params.size.y);
	//color.r = float(offset_pos_red.x) / float(params.size.x);
	//color.g = float(offset_pos_red.y) / float(params.size.y);
	// color.b = 0;
	// color.a = 1;

	imageStore(ca_image, pos, color);
#endif
#ifdef MODE_CA_COMPOSITE
	vec4 color = texture(source_ca, uv);
	imageStore(color_image, pos, color);

#endif
}
