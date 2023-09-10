/**************************************************************************/
/*  chromatic_aberration.h                                                */
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

#ifndef CHROMATIC_ABERRATION_RD_H
#define CHROMATIC_ABERRATION_RD_H

#include "servers/rendering/renderer_rd/pipeline_cache_rd.h"
#include "servers/rendering/renderer_rd/shaders/effects/chromatic_aberration.glsl.gen.h"
//#include "servers/rendering/renderer_rd/shaders/effects/bokeh_dof_raster.glsl.gen.h"
#include "servers/rendering/renderer_scene_render.h"

#include "servers/rendering_server.h"

namespace RendererRD {

class ChromaticAberration {
private:
	//	bool prefer_raster_effects;

	struct ChromaticAberrationPushConstant {
		uint32_t size[2];
		float ca_axial;
		float ca_transverse;

		//	float image_distance;
		//float image_distance_red;
		//float image_distance_green;
		//float image_distance_blue;
		//float focal_length;

		float max_distance;
		float lens_center_line;
		float sensor_half_diagonal;
		float angle_factor;

		float diagonal_half_fov;
		float refract_index_red;
		float refract_index_green;
		float refract_index_blue;

		float apothem;
		float focal_length;
		uint32_t pad[2];

		//float lens_distance_red;
		//float lens_distance_green;
		//float lens_distance_blue;

		// float sensor_size;
		//	uint32_t pad;
		//		uint32_t pad[2];
	};
	enum ChromaticAberrationMode {
		NORMAL,
		COMPOSITE,
		MAX
	};

	//struct ChromaticAb {
	ChromaticAberrationPushConstant push_constant;
	ChromaticAberrationShaderRD ca_shader;
	//	BokehDofRasterShaderRD raster_shader;
	RID shader_version;
	RID pipelines[MAX];
	//		PipelineCacheRD raster_pipelines[BOKEH_MAX];
	//} chromaticab;

public:
	struct ChromaticAberrationBuffers {
		// bokeh buffers

		// textures
		Size2i base_texture_size;
		RID base_texture;
		RID half_texture;

		//	RID depth_texture;
		RID secondary_texture;
		//	RID half_texture[2];
		//
		//	// raster only
		//	RID base_fb;
		//	RID secondary_fb; // with weights
		//	RID half_fb[2]; // with weights
		//	RID base_weight_fb;
		//	RID weight_texture[4];
	};

	// BokehDOF(bool p_prefer_raster_effects);
	ChromaticAberration();
	~ChromaticAberration();

	void chromatic_aberration_process(const ChromaticAberrationBuffers &p_buffers, RID p_camera_attributes);
	//void bokeh_dof_raster(const BokehBuffers &p_buffers, RID p_camera_attributes, float p_cam_znear, float p_cam_zfar, bool p_cam_orthogonal);
};

} // namespace RendererRD

#endif // CHROMATIC_ABERRATION_RD_H