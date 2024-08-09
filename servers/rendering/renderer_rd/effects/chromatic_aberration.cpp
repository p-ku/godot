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
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS_TWO_TONE\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS_THREE_TONE\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS_SPECTRUM\n");
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

void ChromaticAberration::chromatic_aberration_process(RID p_source_texture, RID p_second_texture, Size2i p_full_size, Size2 p_center, RID p_environment) {
	//	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute version of bokeh depth of field with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	bool jitter = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_jitter(p_environment);
	RS::EnvironmentChromaticAberrationSampleMode sample_mode = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_sample_mode(p_environment);
	int samples = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_samples(p_environment);

	float edge_amount = 0.5 * RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_edge_amount(p_environment);
	float minimum_distance = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_minimum_distance(p_environment);
	float diagonal = p_center.length();
	Size2i half_size = p_center.ceil();

	size_t push_constant_size = sizeof(ProcessPushConstant);
	memset(&process_push_constant, 0, push_constant_size);

	process_push_constant.minimum_distance = minimum_distance * diagonal;
	float max_distance = diagonal - process_push_constant.minimum_distance;

	process_push_constant.edge_factor = Math_PI * edge_amount / max_distance;
	process_push_constant.desaturation = RendererSceneRenderRD::get_singleton()->environment_get_chromatic_aberration_desaturation(p_environment);

	process_push_constant.center[0] = p_center.x;
	process_push_constant.center[1] = p_center.y;

	process_push_constant.half_size[0] = half_size.x;
	process_push_constant.half_size[1] = half_size.y;
	process_push_constant.full_size[0] = p_full_size.x;
	process_push_constant.full_size[1] = p_full_size.y;

	// process_push_constant.jitter_seed[0] = 0.5 * Math::randf() - 0.25;
	// process_push_constant.jitter_seed[1] = 0.5 * Math::randf() - 0.25;
	process_push_constant.jitter_seed[0] = Math::randf();
	process_push_constant.jitter_seed[1] = Math::randf();
	// std::cout << "\n"
	// 		  << Math::randf();

	process_push_constant.jitter = jitter;
	Size2 pixel_size = Size2(0.5, 0.5) / half_size;
	process_push_constant.pixel_size[0] = pixel_size.x;
	process_push_constant.pixel_size[1] = pixel_size.y;

	if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_TWO_TONE) {
		process_push_constant.max_samples = 1;
	} else if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_THREE_TONE) {
		process_push_constant.max_samples = 2;
	} else {
		process_push_constant.max_samples = samples;
	}

	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_base_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_texture }));
	RD::Uniform u_secondary_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_second_texture }));

	RD::Uniform u_base_image(RD::UNIFORM_TYPE_IMAGE, 0, p_source_texture);
	RD::Uniform u_secondary_image(RD::UNIFORM_TYPE_IMAGE, 0, p_second_texture);

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	RID shader;
	if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_SPECTRUM) {
		shader = ca_shader.version_get_shader(shader_version, PROCESS_SPECTRUM);
		ERR_FAIL_COND(shader.is_null());
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[PROCESS_SPECTRUM]);
	} else if (sample_mode == RS::ENV_CHROMATIC_ABERRATION_SAMPLE_MODE_THREE_TONE) {
		shader = ca_shader.version_get_shader(shader_version, PROCESS_THREE_TONE);
		ERR_FAIL_COND(shader.is_null());
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[PROCESS_THREE_TONE]);
	} else {
		shader = ca_shader.version_get_shader(shader_version, PROCESS_TWO_TONE);
		ERR_FAIL_COND(shader.is_null());
		RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[PROCESS_TWO_TONE]);
	}

	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_texture), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_secondary_image), 1);

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &process_push_constant, push_constant_size);
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, process_push_constant.half_size[0], process_push_constant.half_size[1], 1);
	RD::get_singleton()->compute_list_add_barrier(compute_list);

	shader = ca_shader.version_get_shader(shader_version, COMPOSITE);
	ERR_FAIL_COND(shader.is_null());
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[COMPOSITE]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_secondary_texture), 1);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &process_push_constant, push_constant_size);
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, process_push_constant.full_size[0], process_push_constant.full_size[1], 1);
	RD::get_singleton()->compute_list_end();
}
