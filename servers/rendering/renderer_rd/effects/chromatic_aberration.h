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
//#include "servers/rendering/renderer_rd/shaders/effects/chromatic_aberration.glsl.gen.h"
#include "scene/resources/gradient_texture.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering_server.h"

// #include "servers/rendering/rendering_server_default.h"

namespace RendererRD {

class ChromaticAberration {
private:
	bool prefer_raster_effects;

	struct ProcessPushConstant {
		int32_t half_size[2];
		int32_t full_size[2];

		float pixel_size[2];
		uint32_t sample_mode;
		uint32_t jitter;

		float jitter_seed[2];
		float max_samples;
		float edge_factor;

		float desaturation;
		float minimum_distance;
		float center[2];
		// uint32_t pad;
	};

	enum ChromaticAberrationMode {
		PROCESS_TWO_TONE,
		PROCESS_THREE_TONE,
		PROCESS_SPECTRUM,
		COMPOSITE,
		MAX_MODES
	};

	//struct ChromaticAb {
	ProcessPushConstant process_push_constant;

	ChromaticAberrationShaderRD ca_shader;
	//	BokehDofRasterShaderRD raster_shader;
	RID shader_version;
	RID pipelines[MAX_MODES];
	//		PipelineCacheRD raster_pipelines[BOKEH_MAX];
	//} chromaticab;

public:
	class ChromaticAberrationBuffers : public RenderBufferCustomDataRD {
		GDCLASS(ChromaticAberrationBuffers, RenderBufferCustomDataRD);

	private:
		bool prefer_raster_effects;

	public:
		void set_prefer_raster_effects(bool p_prefer_raster_effects);
	};

	ChromaticAberration(bool p_prefer_raster_effects);
	~ChromaticAberration();

	void chromatic_aberration_process(RID p_source_texture, RID p_second_texture, Size2i p_full_size, Size2 p_center, RID p_camera_attributes);
};
} // namespace RendererRD
#endif // CHROMATIC_ABERRATION_RD_H