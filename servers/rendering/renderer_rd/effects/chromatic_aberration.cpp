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
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
// #include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering/storage/camera_attributes_storage.h"

using namespace RendererRD;

ChromaticAberration::ChromaticAberration(bool p_prefer_raster_effects) {
	prefer_raster_effects = p_prefer_raster_effects;

	Vector<String> chromatic_aberration_modes;
	chromatic_aberration_modes.push_back("\n#define MODE_CA_SPECTRUM\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_REFRACTION_BOX\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_PROCESS\n");
	chromatic_aberration_modes.push_back("\n#define MODE_CA_COMPOSITE\n");

	ca_shader.initialize(chromatic_aberration_modes);
	shader_version = ca_shader.version_create();
	for (int i = 0; i < MAX; i++) {
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
	refraction_format.width = internal_size.x >> 1;
	refraction_format.height = internal_size.y >> 1;

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

Ref<ChromaticAberration::ChromaticAberrationBuffers> ChromaticAberration::get_buffers(Ref<RenderSceneBuffersRD> p_render_buffers) {
	if (p_render_buffers->has_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS)) {
		return p_render_buffers->get_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS);
	}

	Ref<ChromaticAberrationBuffers> buffers;
	buffers.instantiate();
	buffers->set_prefer_raster_effects(prefer_raster_effects);
	buffers->configure(p_render_buffers.ptr());
	initialize_spectrum_texture(buffers->spectrum);
	// update_refraction_texture(buffers->refraction, p_render_data->camera_attributes, rb.ptr());

	// Ref<RendererRD::ChromaticAberration::ChromaticAberrationBuffers> ca_buffers = get_buffers(rb);
	p_render_buffers->set_custom_data(RB_CHROMATIC_ABERRATION_BUFFERS, buffers);
	return buffers;
}

void ChromaticAberration::initialize_spectrum_texture(RID p_spectrum) {
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	memset(&push_constant, 0, sizeof(ChromaticAberrationPushConstant));
	//	push_constant.size = p_size;
	// float half_diagonal = 0.5 * p_buffers.base_texture_size.length();

	push_constant.size[0] = 5;
	push_constant.size[1] = 1;

	RD::Uniform u_spectrum_buffer(RD::UNIFORM_TYPE_IMAGE, 0, { p_spectrum });

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();
	RID shader = ca_shader.version_get_shader(shader_version, SPECTRUM);
	ERR_FAIL_COND(shader.is_null());
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[SPECTRUM]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_spectrum_buffer), 0);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, push_constant.size[0], push_constant.size[1], 1);

	RD::get_singleton()->compute_list_end();
}

void ChromaticAberration::update_refraction_texture(RID p_refraction, RID p_camera_attributes, Size2i p_size) {
	//	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute version of bokeh depth of field with the mobile renderer.");
	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	float quality = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_quality(p_camera_attributes);

	bool enabled = RSG::camera_attributes->camera_attributes_uses_chromatic_aberration(p_camera_attributes);
	//	float quality = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_quality(p_camera_attributes);
	float curve = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_curve(p_camera_attributes);
	float refract_index_blue = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_refract_index_blue(p_camera_attributes);
	float refract_index_green = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_refract_index_green(p_camera_attributes);
	float intensity = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_intensity(p_camera_attributes);
	float center_offset = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_center_offset(p_camera_attributes);
	// setup our push constant
	memset(&push_constant, 0, sizeof(ChromaticAberrationPushConstant));
	//	push_constant.size = p_size;
	// float half_diagonal = 0.5 * p_buffers.base_texture_size.length();
	push_constant.half_diagonal = p_size.length();
	push_constant.quality = quality; // samples per pixel
	push_constant.center_offset = center_offset * push_constant.half_diagonal;
	push_constant.intensity = 0.5 * Math_PI * intensity / (push_constant.half_diagonal - push_constant.center_offset);
	push_constant.curve = curve;
	// push_constant.intensity = 0.5 * Math_PI * intensity / push_constant.half_diagonal;

	push_constant.size[0] = p_size.x;
	push_constant.size[1] = p_size.y;
	// float intensity_factor = 0.5 * Math_PI * push_constant.half_diagonal * (2.0 - intensity);
	// setup our uniforms

	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_refraction_buffer(RD::UNIFORM_TYPE_IMAGE, 0, { p_refraction });

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	RID shader = ca_shader.version_get_shader(shader_version, REFRACTION_BOX);
	ERR_FAIL_COND(shader.is_null());
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[REFRACTION_BOX]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_refraction_buffer), 0);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, push_constant.size[0], push_constant.size[1], 1);

	RD::get_singleton()->compute_list_end();
}
void ChromaticAberration::chromatic_aberration_process(RID p_source_texture, RID p_second_texture, const Size2i p_source_size, Ref<RenderSceneBuffersRD> p_render_buffers, Ref<ChromaticAberrationBuffers> p_buffers, RID p_camera_attributes) {
	//	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute version of bokeh depth of field with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	bool enabled = RSG::camera_attributes->camera_attributes_uses_chromatic_aberration(p_camera_attributes);
	//float quality = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_quality(p_camera_attributes);
	//float intensity = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_intensity(p_camera_attributes);
	float lens_distance = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_lens_distance(p_camera_attributes);
	float refract_index_blue = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_refract_index_blue(p_camera_attributes);
	float refract_index_green = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_refract_index_green(p_camera_attributes);
	float center_offset = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_center_offset(p_camera_attributes);

	// setup our push constant
	memset(&push_constant, 0, sizeof(ChromaticAberrationPushConstant));

	// setup our uniforms
	//	push_constant.quality = quality; // samples per pixel

	push_constant.size[0] = p_source_size.x;
	push_constant.size[1] = p_source_size.y;
	//	push_constant.half_diagonal = 0.5 * p_buffers.base_texture_size.length();
	push_constant.lens_distance = lens_distance; // * push_constant.half_diagonal;
	//push_constant.quality = quality;
	//push_constant.intensity = intensity;
	push_constant.half_diagonal = 0.5 * p_source_size.length();
	push_constant.center_offset = center_offset * push_constant.half_diagonal;
	push_constant.jitter_seed = Math::randf() * 1000.0;
	push_constant.half_pixel_size[0] = 0.5 / p_source_size.x;
	push_constant.half_pixel_size[1] = 0.5 / p_source_size.y;

	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_base_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_source_texture }));
	RD::Uniform u_secondary_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_second_texture }));
	// RD::Uniform u_half_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.half_texture }));
	RD::Uniform u_refraction_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers->refraction }));
	RD::Uniform u_spectrum_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers->spectrum }));

	RD::Uniform u_base_image(RD::UNIFORM_TYPE_IMAGE, 0, p_source_texture);
	RD::Uniform u_secondary_image(RD::UNIFORM_TYPE_IMAGE, 0, p_render_buffers->get_texture_slice(RB_SCOPE_BUFFERS, RB_TEX_BLUR_0, 0, 0));

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

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_source_size.x, p_source_size.y, 1);
	RD::get_singleton()->compute_list_add_barrier(compute_list);

	shader = ca_shader.version_get_shader(shader_version, COMPOSITE);
	ERR_FAIL_COND(shader.is_null());
	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[COMPOSITE]);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_secondary_texture), 1);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_source_size.x, p_source_size.y, 1);

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