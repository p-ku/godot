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
#include "core/os/time.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/renderer_scene_render.h"
#include <iostream>

using namespace RendererRD;

ChromaticAberration::ChromaticAberration(bool p_prefer_raster_effects) {
	prefer_raster_effects = p_prefer_raster_effects;

	Vector<String> chromatic_aberration_modes;
	chromatic_aberration_modes.push_back("\n#define MODE_CA_SPECTRUM\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_REFRACTION_BOX\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS_TWO_TONE\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS_THREE_TONE\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS_SPECTRUM_DEFAULT\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS_SPECTRUM_CUSTOM\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_COMPOSITE\n");

	ca_shader.initialize(chromatic_aberration_modes);
	shader_version = ca_shader.version_create();
	for (int i = 0; i < MAX_MODES; i++) {
		pipelines[i] = RD::get_singleton()->compute_pipeline_create(ca_shader.version_get_shader(shader_version, i));
	}
}
ChromaticAberration::~ChromaticAberration() {
	ca_shader.version_free(shader_version);
}

void ChromaticAberration::ChromaticAberrationBuffers::set_prefer_raster_effects(bool p_prefer_raster_effects) {
	prefer_raster_effects = p_prefer_raster_effects;
}

void ChromaticAberration::initialize_spectrum_texture(RID p_spectrum) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	size_t push_constant_size = sizeof(SpectrumPushConstant);
	memset(&spectrum_push_constant, 0, push_constant_size);

	spectrum_push_constant.size[0] = 5;
	spectrum_push_constant.size[1] = 1;

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
	RS::EnvironmentChromaticAberrationSampleMode sample_mode = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_sample_mode(p_environment);
	bool jitter = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_jitter(p_environment);
	int samples = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_samples(p_environment);

	float edge_amount = 0.5 * RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_edge_amount(p_environment);
	float linear_amount = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_linear_amount(p_environment);
	//	Vector2 center = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_center(p_environment);
	//	center.y = 1.0 - center.y;
	float minimum_distance = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_minimum_distance(p_environment);

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

	float max_blue_distance = refraction_push_constant.minimum_distance + refraction_push_constant.linear_factor * sin(max_distance * refraction_push_constant.edge_factor) / refraction_push_constant.edge_factor;
	float max_pixel_range = max_distance - max_blue_distance;

	float epsilon = 0.0078125; // based on 8-bit color (2 / 256), anything smaller is imperceptible
	//#define EPSILON 1.017812 // based on ?

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
	refraction_push_constant.sample_mode = sample_mode;

	refraction_push_constant.half_size[0] = p_half_size.x;
	refraction_push_constant.half_size[1] = p_half_size.y;
	refraction_push_constant.full_size[0] = p_full_size.x;
	refraction_push_constant.full_size[1] = p_full_size.y;
	if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_TWO_TONE) {
		refraction_push_constant.max_samples = 1;
	} else if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_THREE_TONE) {
		refraction_push_constant.max_samples = 2;
	} else {
		refraction_push_constant.max_samples = samples;
	}

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
void ChromaticAberration::chromatic_aberration_process(RID p_source_texture, RID p_second_texture, Size2i p_half_size, Size2i p_full_size, Size2 p_center, float p_diagonal, RID p_environment, RID p_custom_spectrum_texture) {
	//	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute version of bokeh depth of field with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);
	// int samples = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_samples(p_environment);
	//	Vector2 center = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_center(p_environment);
	//	center.y = 1.0 - center.y;
	bool jitter = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_jitter(p_environment);
	RS::EnvironmentChromaticAberrationSampleMode sample_mode = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_sample_mode(p_environment);
	int samples = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_samples(p_environment);

	float edge_amount = 0.5 * RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_edge_amount(p_environment);
	float linear_amount = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_linear_amount(p_environment);
	//	Vector2 center = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_center(p_environment);
	//	center.y = 1.0 - center.y;
	float minimum_distance = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_minimum_distance(p_environment);
	// RID custom_tex = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_custom_texture(p_environment);

	//RID custom_tex = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_custom_texture(p_environment);
	//	float edge_amount = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_edge_amount(p_environment);
	//	float linear_amount = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_linear_amount(p_environment);
	// float minimum_distance = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_minimum_distance(p_environment);

	// setup our push constant
	size_t push_constant_size = sizeof(ProcessPushConstant);
	memset(&process_push_constant, 0, push_constant_size);

	// setup our uniforms
	//	push_constant.samples = samples; // samples per pixel
	//	process_push_constant.pixel_count = p_half_size.x * p_half_size.y;
	process_push_constant.minimum_distance = minimum_distance * p_diagonal;
	// push_constant.minimum_distance = minimum_distance * push_constant.diagonal;

	//push_constant.minimum_distance = minimum_distance * push_constant.diagonal;
	float max_distance = p_diagonal - refraction_push_constant.minimum_distance;

	process_push_constant.edge_factor = Math_PI * edge_amount / max_distance;
	process_push_constant.linear_factor = 1.0 - linear_amount;

	//	Vector2 pixel_center = center * p_size;
	process_push_constant.center[0] = p_center.x;
	process_push_constant.center[1] = p_center.y;

	process_push_constant.half_size[0] = p_half_size.x;
	process_push_constant.half_size[1] = p_half_size.y;
	process_push_constant.full_size[0] = p_full_size.x;
	process_push_constant.full_size[1] = p_full_size.y;
	//	push_constant.diagonal = 0.5 * p_buffers.base_texture_size.length();
	//push_constant.intensity = intensity;
	//	push_constant.diagonal = p_diagonal;

	process_push_constant.jitter_seed[0] = 0.5 * Math::randf() - 0.25;
	process_push_constant.jitter_seed[1] = 0.5 * Math::randf() - 0.25;
	// process_push_constant.tritone = tritone;

	process_push_constant.jitter = jitter;
	//	push_constant.time = Time::get_singleton()->get_ticks_msec();
	Size2 pixel_size = Size2(0.5, 0.5) / p_half_size;
	process_push_constant.pixel_size[0] = pixel_size.x;
	process_push_constant.pixel_size[1] = pixel_size.y;

	//	process_push_constant.min_uv_delta = pixel_size.length() / 3;
	if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_TWO_TONE) {
		process_push_constant.max_samples = 1;
	} else if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_THREE_TONE) {
		process_push_constant.max_samples = 2;
	} else {
		process_push_constant.max_samples = samples;
	}

	//	process_push_constant.max_samples = MAX(3, MIN(p_diagonal, 1023) * quality);
	// process_push_constant.max_samples = samples;

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
	// RD::Uniform u_refraction_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers->refraction }));
	// RD::Uniform u_spectrum_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers->spectrum }));
	RD::Uniform u_spectrum_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, custom_texture }));

	RD::Uniform u_base_image(RD::UNIFORM_TYPE_IMAGE, 0, p_source_texture);
	//	RD::Uniform u_refraction_image(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers->refraction);

	RD::Uniform u_secondary_image(RD::UNIFORM_TYPE_IMAGE, 0, p_second_texture);

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	//
	//RID shader = ca_shader.version_get_shader(shader_version, REFRACTION);
	//ERR_FAIL_COND(shader.is_null());
	//RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[REFRACTION]);
	//RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_half_image), 0);
	//RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));
	//RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_buffers.base_texture_size.x, 1, 1);
	//RD::get_singleton()->compute_list_add_barrier(compute_list);
	RID shader;
	if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_TWO_TONE) {
		shader = ca_shader.version_get_shader(shader_version, PROCESS_TWO_TONE);
		ERR_FAIL_COND(shader.is_null());
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[PROCESS_TWO_TONE]);
	} else if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_THREE_TONE) {
		shader = ca_shader.version_get_shader(shader_version, PROCESS_THREE_TONE);
		ERR_FAIL_COND(shader.is_null());
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[PROCESS_THREE_TONE]);
	} else {
		if (p_custom_spectrum_texture.is_valid()) {
			shader = ca_shader.version_get_shader(shader_version, PROCESS_SPECTRUM_CUSTOM);
			ERR_FAIL_COND(shader.is_null());
			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[PROCESS_SPECTRUM_CUSTOM]);
			RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 2, u_spectrum_texture), 2);
		} else {
			shader = ca_shader.version_get_shader(shader_version, PROCESS_SPECTRUM_DEFAULT);
			ERR_FAIL_COND(shader.is_null());
			RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[PROCESS_SPECTRUM_DEFAULT]);
		}
	}

	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_texture), 0);
	// RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_refraction_texture), 1);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_secondary_image), 1);
	//	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 4, u_base_image), 4);
	//	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 5, u_refraction_image), 5);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &process_push_constant, push_constant_size);
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, process_push_constant.half_size[0], process_push_constant.half_size[1], 1);
	RD::get_singleton()->compute_list_add_barrier(compute_list);
	// process_push_constant.full_size[0] = p_full_size.x;
	// process_push_constant.full_size[1] = p_full_size.y;
	shader = ca_shader.version_get_shader(shader_version, COMPOSITE);
	ERR_FAIL_COND(shader.is_null());
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[COMPOSITE]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_secondary_texture), 1);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &process_push_constant, push_constant_size);
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, process_push_constant.full_size[0], process_push_constant.full_size[1], 1);

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