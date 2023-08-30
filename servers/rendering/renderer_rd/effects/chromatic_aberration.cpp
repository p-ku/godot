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
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/storage/camera_attributes_storage.h"

using namespace RendererRD;

ChromaticAberration::ChromaticAberration() {
	Vector<String> chromatic_aberration_modes;
	chromatic_aberration_modes.push_back("\n#define MODE_CA_NORMAL\n");
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

void ChromaticAberration::chromatic_aberration_process(const ChromaticAberrationBuffers &p_buffers, RID p_camera_attributes) {
	//	ERR_FAIL_COND_MSG(prefer_raster_effects, "Can't use compute version of bokeh depth of field with the mobile renderer.");

	UniformSetCacheRD *uniform_set_cache = UniformSetCacheRD::get_singleton();
	ERR_FAIL_NULL(uniform_set_cache);
	MaterialStorage *material_storage = MaterialStorage::get_singleton();
	ERR_FAIL_NULL(material_storage);

	//	bool dof_far = RSG::camera_attributes->camera_attributes_get_dof_far_enabled(p_camera_attributes);
	//	float dof_far_begin = RSG::camera_attributes->camera_attributes_get_dof_far_distance(p_camera_attributes);
	//	float dof_far_size = RSG::camera_attributes->camera_attributes_get_dof_far_transition(p_camera_attributes);
	//	bool dof_near = RSG::camera_attributes->camera_attributes_get_dof_near_enabled(p_camera_attributes);
	//	float dof_near_begin = RSG::camera_attributes->camera_attributes_get_dof_near_distance(p_camera_attributes);
	//	float dof_near_size = RSG::camera_attributes->camera_attributes_get_dof_near_transition(p_camera_attributes);
	//	float bokeh_size = RSG::camera_attributes->camera_attributes_get_dof_blur_amount(p_camera_attributes) * 64; // Base 64 pixel radius.

	float ca_axial_amount = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_axial_amount(p_camera_attributes);
	float ca_transverse_amount = RSG::camera_attributes->camera_attributes_get_chromatic_aberration_transverse_amount(p_camera_attributes);

	//	bool use_jitter = RSG::camera_attributes->camera_attributes_get_dof_blur_use_jitter();
	//	RS::DOFBokehShape bokeh_shape = RSG::camera_attributes->camera_attributes_get_dof_blur_bokeh_shape();
	//	RS::DOFBlurQuality blur_quality = RSG::camera_attributes->camera_attributes_get_dof_blur_quality();

	// setup our push constant
	memset(&push_constant, 0, sizeof(ChromaticAberrationPushConstant));
	push_constant.ca_axial = ca_axial_amount;
	push_constant.ca_transverse = ca_transverse_amount;

	//	bokeh.push_constant.blur_far_active = dof_far;
	//	bokeh.push_constant.blur_far_begin = dof_far_begin;
	//	bokeh.push_constant.blur_far_end = dof_far_begin + dof_far_size; // Only used with non-physically-based.
	//	bokeh.push_constant.use_physical_far = dof_far_size < 0.0;
	//	bokeh.push_constant.blur_size_far = bokeh_size; // Only used with physically-based.
	//
	//	bokeh.push_constant.blur_near_active = dof_near;
	//	bokeh.push_constant.blur_near_begin = dof_near_begin;
	//	bokeh.push_constant.blur_near_end = dof_near_begin - dof_near_size; // Only used with non-physically-based.
	//	bokeh.push_constant.use_physical_near = dof_near_size < 0.0;
	//	bokeh.push_constant.blur_size_near = bokeh_size; // Only used with physically-based.
	//
	//	bokeh.push_constant.use_jitter = use_jitter;
	//	bokeh.push_constant.jitter_seed = Math::randf() * 1000.0;
	//
	//	bokeh.push_constant.z_near = p_cam_znear;
	//	bokeh.push_constant.z_far = p_cam_zfar;
	//	bokeh.push_constant.orthogonal = p_cam_orthogonal;
	//	bokeh.push_constant.blur_size = (dof_near_size < 0.0 && dof_far_size < 0.0) ? 32 : bokeh_size; // Cap with physically-based to keep performance reasonable.

	//	bokeh.push_constant.second_pass = false;
	//	bokeh.push_constant.half_size = false;

	//	bokeh.push_constant.blur_scale = 0.5;

	// setup our uniforms
	RID default_sampler = material_storage->sampler_rd_get_default(RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	RD::Uniform u_base_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.base_texture }));
	//	RD::Uniform u_depth_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.depth_texture }));
	RD::Uniform u_secondary_texture(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.secondary_texture }));
	//	RD::Uniform u_half_texture0(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.half_texture[0] }));
	//	RD::Uniform u_half_texture1(RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE, 0, Vector<RID>({ default_sampler, p_buffers.half_texture[1] }));

	RD::Uniform u_base_image(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers.base_texture);
	RD::Uniform u_secondary_image(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers.secondary_texture);
	//	RD::Uniform u_half_image0(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers.half_texture[0]);
	//	RD::Uniform u_half_image1(RD::UNIFORM_TYPE_IMAGE, 0, p_buffers.half_texture[1]);

	RD::ComputeListID compute_list = RD::get_singleton()->compute_list_begin();

	/* FIRST PASS */
	// The alpha channel of the source color texture is filled with the expected circle size
	// If used for DOF far, the size is positive, if used for near, its negative.
	RID shader = ca_shader.version_get_shader(shader_version, NORMAL);
	ERR_FAIL_COND(shader.is_null());

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[NORMAL]);

	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_secondary_image), 0);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_base_texture), 1);

	// RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_depth_texture), 1);

	push_constant.size[0] = p_buffers.base_texture_size.x;
	push_constant.size[1] = p_buffers.base_texture_size.y;

	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));

	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_buffers.base_texture_size.x, p_buffers.base_texture_size.y, 1);
	RD::get_singleton()->compute_list_add_barrier(compute_list);

	shader = ca_shader.version_get_shader(shader_version, COMPOSITE);
	ERR_FAIL_COND(shader.is_null());

	RD::get_singleton()->compute_list_bind_compute_pipeline(compute_list, pipelines[COMPOSITE]);

	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 1, u_secondary_texture), 1);
	RD::get_singleton()->compute_list_bind_uniform_set(compute_list, uniform_set_cache->get_cache(shader, 0, u_base_image), 0);
	RD::get_singleton()->compute_list_set_push_constant(compute_list, &push_constant, sizeof(ChromaticAberrationPushConstant));
	RD::get_singleton()->compute_list_dispatch_threads(compute_list, p_buffers.base_texture_size.x, p_buffers.base_texture_size.y, 1);

	RD::get_singleton()->compute_list_end();
}
