/**************************************************************************/
/*  chromatic_aberration.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "chromatic_aberration.h"
#include "copy_effects.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
// #include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "core/os/time.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/renderer_scene_render.h"
#include <iostream>

// #include "servers/rendering/rendering_server_default.h"
// #include "servers/rendering/storage/environment_storage.h"

using namespace RendererRD;

ChromaticAberration::ChromaticAberration(bool p_prefer_raster_effects) {
	prefer_raster_effects = p_prefer_raster_effects;

	Vector<String> chromatic_aberration_modes;
	chromatic_aberration_modes.push_back("\n#define MODE_CA_SPECTRUM\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_REFRACTION_BOX\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS\n");
	//	chromatic_aberration_modes.push_back("\n#define MODE_CA_COMPOSITE\n");

	ca_shader.initialize(chromatic_aberration_modes);
	shader_version = ca_shader.version_create();
	for (int i = 0; i < MAX_MODES; i++) {
		pipelines[i] = RD::get_singleton()->compute_pipeline_create(ca_shader.version_get_shader(shader_version, i));
	}

	//pipeline = RD::get_singleton()->compute_pipeline_create(ca_shader.version_get_shader(shader_version, 0));
}
ChromaticAberration::~ChromaticAberration() {
	ca_shader.version_free(shader_version);
}

void ChromaticAberration::ChromaticAberrationBuffers::set_prefer_raster_effects(bool p_prefer_raster_effects) {
	prefer_raster_effects = p_prefer_raster_effects;
}

void ChromaticAberration::ChromaticAberrationBuffers::configure(RenderSceneBuffersRD *p_render_buffers) {
	Size2i internal_size = p_render_buffers->get_internal_size();

	RD::TextureFormat spectrum_format;
	RD::TextureFormat refraction_format;
	spectrum_format.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
	spectrum_format.width = 5;
	spectrum_format.height = 1;

	refraction_format.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
	//refraction_format.format = RD::DATA_FORMAT_R64G64B64A64_SFLOAT;

	// refraction_format.width = internal_size.x;
	// refraction_format.height = internal_size.y;
	//	refraction_format.width = (3 + internal_size.x) >> 1; // Adding one effectively rounds up.
	//	refraction_format.height = (3 + internal_size.y) >> 1;
	refraction_format.width = (internal_size.x % 2) + (internal_size.x >> 1);
	refraction_format.height = (internal_size.y % 2) + (internal_size.y >> 1);

	// refraction_format.width = ceil(internal_size.x * 0.5);
	// refraction_format.height = ceil(internal_size.y * 0.5);
	//	refraction_format.width = (internal_size.x + 1) / 2;
	//	refraction_format.height = (internal_size.y + 1) / 2;

	if (prefer_raster_effects) {
		spectrum_format.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
		refraction_format.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
	} else {
		spectrum_format.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
		refraction_format.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
	}
	spectrum = RD::get_singleton()->texture_create(spectrum_format, RD::TextureView());
	refraction = RD::get_singleton()->texture_create(refraction_format, RD::TextureView());
}

void ChromaticAberration::ChromaticAberrationBuffers::free_data() {
	if (spectrum.is_valid()) {
		RD::get_singleton()->free(spectrum);
		spectrum = RID();
	}

	if (refraction.is_valid()) {
		RD::get_singleton()->free(refraction);
		refraction = RID();
	}
}

Ref<ChromaticAberration::ChromaticAberrationBuffers> ChromaticAberration::get_buffers(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_environment) {
	Ref<ChromaticAberrationBuffers> buffers;

	if (p_render_buffers->has_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS)) {
		buffers = p_render_buffers->get_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS);
		//	return p_render_buffers->get_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS);
	} else {
		buffers.instantiate();
		buffers->set_prefer_raster_effects(prefer_raster_effects);
		buffers->configure(p_render_buffers.ptr());
		// update_refraction_texture(buffers->refraction, p_render_data->environment, rb.ptr());

		// Ref<RendererRD::ChromaticAberration::ChromaticAberrationBuffers> ca_buffers = get_buffers(rb);
		p_render_buffers->set_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS, buffers);
	}
	//	initialize_spectrum_texture(p_environment, buffers->spectrum);

	return buffers;
}

void ChromaticAberration::initialize_spectrum_texture(RID p_environment, RID p_spectrum) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	//	push_constant.size = p_size;
	// float diagonal = 0.5 * p_buffers.base_texture_size.length();
	float desaturation = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_desaturation(p_environment);
	size_t push_constant_size = sizeof(SpectrumPushConstant);
	memset(&spectrum_push_constant, 0, push_constant_size);

	//	int samp = 3;
	//	push_constant.diagonal = 2.4;
	//	push_constant.samples = samp;
	//	push_constant.edge_amount = 0.0;
	//	push_constant.linear_amount = 0.0;
	//
	//	push_constant.center[0] = 0.0;
	//	push_constant.center[1] = 0.0;
	//	push_constant.minimum_distance = 0.0;
	spectrum_push_constant.desaturation = desaturation;

	//push_constant.horizontal_smear = 0.0;
	//push_constant.vertical_smear = 0.0;
	//push_constant.half_pixel_size[0] = 3.0;
	//push_constant.half_pixel_size[1] = 1.0;
	//
	//push_constant.jitter_amount = 0.0;
	spectrum_push_constant.size[0] = 5;
	spectrum_push_constant.size[1] = 1;
	//push_constant.jitter_seed = Math::randf() * 1000.0;

	RD::Uniform u_spectrum_buffer(RD::UNIFORM_TYPE_IMAGE, 0, { p_spectrum });

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	RID shader = ca_shader.version_get_shader(shader_version, SPECTRUM);
	ERR_FAIL_COND(shader.is_null());
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[SPECTRUM]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_spectrum_buffer), 0);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &spectrum_push_constant, push_constant_size);
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, spectrum_push_constant.size[0], spectrum_push_constant.size[1], 1);
	RD::get_singleton()->compute_list_end();
}

void ChromaticAberration::update_refraction_texture(RID p_refraction, RID p_environment, Size2i p_half_size, Size2i p_full_size, Size2 p_center, float p_diagonal) {
	//	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute version of bokeh depth of field with the mobile renderer.");
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	float quality = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_quality(p_environment);
	float edge_amount = 0.5 * RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_edge_amount(p_environment);
	float linear_amount = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_linear_amount(p_environment);
	//	Vector2 center = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_center(p_environment);
	//	center.y = 1.0 - center.y;
	bool circular = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_circular(p_environment);
	float minimum_distance = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_minimum_distance(p_environment);
	float horizontal_smear = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_horizontal_smear(p_environment);

	// setup our push constant
	size_t push_constant_size = sizeof(RefractionPushConstant);
	memset(&refraction_push_constant, 0, push_constant_size);
	//	push_constant.size = p_size;
	// float diagonal = 0.5 * p_buffers.base_texture_size.length();
	//	int32_t long_dim = MAX(p_size.x, p_size.y);
	//push_constant.long_dim = MAX(p_half_size.x, p_half_size.y);
	// int32_t short_dim = MIN(p_size.x, p_size.y);

	int32_t short_dim = MIN(p_half_size.x, p_half_size.y);
	refraction_push_constant.spiral_pixel_count = short_dim * short_dim;
	//	refraction_push_constant.diagonal = p_diagonal;
	//	push_constant.minimum_distance = MAX(256, minimum_distance * push_constant.diagonal);
	refraction_push_constant.minimum_distance = minimum_distance * p_diagonal;
	// push_constant.minimum_distance = minimum_distance * push_constant.diagonal;

	//push_constant.minimum_distance = minimum_distance * push_constant.diagonal;
	float max_distance = p_diagonal - refraction_push_constant.minimum_distance;

	refraction_push_constant.edge_factor = Math_PI * edge_amount / max_distance;
	refraction_push_constant.linear_factor = 1.0 - linear_amount;

	//	Vector2 pixel_center = center * p_size;
	refraction_push_constant.center[0] = p_center.x;
	refraction_push_constant.center[1] = p_center.y;
	Vector2 center_dir = p_center.normalized();
	refraction_push_constant.center_dir[0] = center_dir.x;
	refraction_push_constant.center_dir[1] = center_dir.y;
	//	Vector2 center_diff = p_size - p_center;
	//	center_diff = Vector2(0, 0);
	//	center_diff = center_diff.max(p_center);
	// if (center_diff.x < pixel_center.x) {
	// 	center_diff.x = pixel_center.x;
	// }
	// if (center_diff.y < pixel_center.y) {
	// 	center_diff.y = pixel_center.y;
	// }

	//	float blue_center_distance = params.minimum_distance + params.linear_factor * sin((pixel_center_distance - params.minimum_distance) * params.edge_factor) / params.edge_factor;
	//	float sample_pixel_range = pixel_center_distance - blue_center_distance;
	// float max_blue_distance = push_constant.minimum_distance;
	// if (edge_amount > 0.0) {
	// float max_distance = center_diff.length() - push_constant.minimum_distance;

	float max_blue_distance = refraction_push_constant.minimum_distance + linear_amount * sin(max_distance * refraction_push_constant.edge_factor) / refraction_push_constant.edge_factor;

	//push_constant.max_samples = MAX(3.0, quality * (max_distance - max_blue_distance) / (3.0 * 1.4142136));
	//	push_constant.center[0] = 0.0;
	//	push_constant.center[1] = 0.0;

	// push_constant.intensity = 0.5 * Math_PI * intensity / push_constant.diagonal;

	// std::cout << push_constant.short_dim << " short"
	// 		  << "\n";
	// std::cout << push_constant.long_dim << " long"
	// 		  << "\n";
	// std::cout << push_constant.short_dim << " full short"
	// 		  << "\n";
	// std::cout << push_constant.long_dim << " full long"
	// 		  << "\n";
	// std::cout << push_constant.spiral_pixel_count << " count"
	// 		  << "\n";
	//	std::cout << p_full_size.x << " x_dim_full | " << p_full_size.y << " y_dim_full | " << p_half_size.x << " x_dim_half | " << p_half_size.y << " y_dim_half"
	//			  << "\n";
	refraction_push_constant.landscape = p_full_size.x > p_full_size.y;
	refraction_push_constant.pixel_count = p_half_size.x * p_half_size.y;
	refraction_push_constant.half_size[0] = p_half_size.x;
	refraction_push_constant.half_size[1] = p_half_size.y;
	refraction_push_constant.full_size[0] = p_full_size.x;
	refraction_push_constant.full_size[1] = p_full_size.y;

	// refraction_push_constant.max_samples = 1000;

	// push_constant.horizontal_smear = horizontal_smear;

	// uint32_t magnitude = 1;
	// while (magnitude < push_constant.pixel_count) {
	// 	magnitude *= 10;
	// }
	// push_constant.magnitude = magnitude;
	// float intensity_factor = 0.5 * Math_PI * push_constant.diagonal * (2.0 - intensity);
	// setup our uniforms

	// RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_refraction_buffer(RD::UNIFORM_TYPE_IMAGE, 0, { p_refraction });

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	RID shader = ca_shader.version_get_shader(shader_version, REFRACTION_BOX);
	ERR_FAIL_COND(shader.is_null());
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[REFRACTION_BOX]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_refraction_buffer), 0);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &refraction_push_constant, push_constant_size);
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, refraction_push_constant.half_size[0], refraction_push_constant.half_size[1], 1);

	RD::get_singleton()->compute_list_end();
}
void ChromaticAberration::chromatic_aberration_process(RID p_source_texture, RID p_second_texture, Size2i p_half_size, Size2i p_full_size, Size2 p_center, float p_diagonal, Ref<RenderSceneBuffersRD> p_render_buffers, Ref<ChromaticAberrationBuffers> p_buffers, RID p_environment) {
	//	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute version of bokeh depth of field with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);
	float quality = 0.1 * RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_quality(p_environment);

	//	Vector2 center = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_center(p_environment);
	//	center.y = 1.0 - center.y;
	float jitter_amount = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_jitter_amount(p_environment);

	//	float edge_amount = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_edge_amount(p_environment);
	//	float linear_amount = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_linear_amount(p_environment);
	// float minimum_distance = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_minimum_distance(p_environment);

	// setup our push constant
	size_t push_constant_size = sizeof(ProcessPushConstant);
	memset(&process_push_constant, 0, push_constant_size);

	// setup our uniforms
	//	push_constant.samples = samples; // samples per pixel
	//	process_push_constant.pixel_count = p_half_size.x * p_half_size.y;

	process_push_constant.half_size[0] = p_half_size.x;
	process_push_constant.half_size[1] = p_half_size.y;
	process_push_constant.full_size[0] = p_full_size.x;
	process_push_constant.full_size[1] = p_full_size.y;
	//	push_constant.diagonal = 0.5 * p_buffers.base_texture_size.length();
	//push_constant.intensity = intensity;
	//	push_constant.diagonal = p_diagonal;
	process_push_constant.jitter_seed[0] = 0.5 * Math::randf() - 0.25;
	process_push_constant.jitter_seed[1] = 0.5 * Math::randf() - 0.25;

	process_push_constant.jitter_amount = jitter_amount;
	//	push_constant.time = Time::get_singleton()->get_ticks_msec();
	Size2 pixel_size = Size2(0.5, 0.5) / p_half_size;
	process_push_constant.pixel_size[0] = pixel_size.x;
	process_push_constant.pixel_size[1] = pixel_size.y;

	process_push_constant.min_uv_delta = pixel_size.length() / 3;

	process_push_constant.max_samples = MAX(3, MIN(p_diagonal, 1023) * quality);
	// Size2 base_uv_delta = pixel_size / 3.0;
	//	process_push_constant.center[0] = p_center.x;
	//	process_push_constant.center[1] = p_center.y;

	//	push_constant.spiral_pixel_count = MIN(p_half_size.x, p_half_size.y) * MIN(p_half_size.x, p_half_size.y);
	//	push_constant.minimum_distance = MAX(256, minimum_distance * push_constant.diagonal);
	//	process_push_constant.minimum_distance = minimum_distance * p_diagonal;
	// push_constant.minimum_distance = minimum_distance * push_constant.diagonal;
	//push_constant.minimum_distance = minimum_distance * push_constant.diagonal;
	// float max_distance = p_diagonal - process_push_constant.minimum_distance;
	// push_constant.edge_factor = Math_PI * edge_amount / max_distance;
	// push_constant.linear_factor = 1.0 - linear_amount;
	// float max_blue_distance;
	// if (push_constant.edge_factor != 0.0) {
	// 	max_blue_distance = push_constant.minimum_distance + linear_amount * sin(max_distance * push_constant.edge_factor) / push_constant.edge_factor;
	// } else {
	// 	max_blue_distance = push_constant.minimum_distance + linear_amount * max_distance;
	// }
	// push_constant.max_samples = MAX(3.0, quality * (max_distance - max_blue_distance) / (3.0 * 1.4142136));
	//	push_constant.max_samples = int32_t(quality * 1000);

	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_base_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_texture }));
	RD::Uniform u_secondary_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_second_texture }));
	// RD::Uniform u_half_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.half_texture }));
	RD::Uniform u_refraction_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers->refraction }));
	RD::Uniform u_spectrum_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers->spectrum }));

	RD::Uniform u_base_image(RD::UNIFORM_TYPE_IMAGE, 0, p_source_texture);
	//	RD::Uniform u_refraction_image(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers->refraction);

	RD::Uniform u_secondary_image(RD::UNIFORM_TYPE_IMAGE, 0, p_render_buffers->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_1, 0, 0));

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	//
	//RID shader = ca_shader.version_get_shader(shader_version, REFRACTION);
	//ERR_FAIL_COND(shader.is_null());
	//RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[REFRACTION]);
	//RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_half_image), 0);
	//RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));
	//RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_buffers.base_texture_size.x, 1, 1);
	//RD::get_singleton()->compute_list_add_barrier(compute_list);

	RID shader = ca_shader.version_get_shader(shader_version, PROCESS);
	ERR_FAIL_COND(shader.is_null());
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[PROCESS]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_refraction_texture), 1);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 2, u_spectrum_texture), 2);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 3, u_secondary_image), 3);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 4, u_base_image), 4);
	//	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 5, u_refraction_image), 5);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &process_push_constant, push_constant_size);
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, process_push_constant.half_size[0], process_push_constant.half_size[1], 1);
	//	RD::get_singleton()->compute_list_add_barrier(compute_list);
	//
	//	shader = ca_shader.version_get_shader(shader_version, COMPOSITE);
	//	ERR_FAIL_COND(shader.is_null());
	//	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[COMPOSITE]);
	//	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
	//	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_secondary_texture), 1);
	//	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));
	//	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_source_size.x, p_source_size.y, 1);

	RD::get_singleton()->compute_list_end();
}
//void ChromaticAberration::CACustomBuffers::free_data() {
//	for (int i = 0; i < reduce.size(); i++) {
//		RD::get_singleton()->free(reduce[i]);
//	}
//	reduce.clear();
//	if (current.is_valid()) {
//		RD::get_singleton()->free(current);
//		current = RID();
//	}
//}
// Ref<ChromaticAberration::CACustomBuffers> ChromaticAberration::get_ca_buffers(Ref<RenderSceneBuffersRD> p_render_buffers) {
// 	if (p_render_buffers->has_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS)) {
// 		return p_render_buffers->get_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS);
// 	}
// 	Ref<CACustomBuffers> buffers;
// 	buffers.instantiate();
// 	buffers->set_prefer_raster_effects(prefer_raster_effects);
// 	buffers->configure(p_render_buffers.ptr());
// 	p_render_buffers->set_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS, buffers);
// 	return buffers;
// }
//RID ChromaticAberration::get_current_ca_buffer(Ref<RenderSceneBuffersRD> p_render_buffers) {
//	if (p_render_buffers->has_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS)) {
//		Ref<CACustomBuffers> buffers = p_render_buffers->get_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS);
//		return buffers->current;
//	}
//	return RID();
//}