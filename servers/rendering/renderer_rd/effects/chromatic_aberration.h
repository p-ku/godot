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
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering_server.h"

#include "servers/rendering/rendering_server_default.h"

#define RB_CHROMATIC_ABERRATION_BUFFERS SNAME("chromatic_aberration_buffers")
// #define REFRACTION_BUFFER SNAME("bend_buffer")
// #define SPECTRUM_BUFFER SNAME("spectrum_buffer")

// #define RB_CHROMATIC_ABERRATION_BUFFERS_EMPTY SNAME("chromatic_aberration_buffers_empty")

namespace RendererRD {

class ChromaticAberration {
private:
	bool prefer_raster_effects;
	bool updated = true;

	struct ChromaticAberrationPushConstant {
		float half_diagonal;
		float lens_distance;
		float refract_index[2];
		//float refract_index_green;

		float quality;
		float center_offset;
		float half_pixel_size[2];

		float jitter_seed;
		float time;
		uint32_t size[2];

		float intensity;
		float curve;
		uint32_t pad[2];
	};
	enum ChromaticAberrationMode {
		SPECTRUM,
		//REFRACTION_LINE,
		REFRACTION_BOX,
		PROCESS,
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
	//	struct ChromaticAberrationBuffers {
	//		// bokeh buffers
	//		// textures
	//		Size2i base_texture_size;
	//		Size2i half_texture_size;
	//		u_int32_t half_diagonal;
	//
	//		RID base_texture;
	//		RID half_texture;
	//		// RID half_texture;
	//		RID secondary_texture;
	//		RID refraction_texture;
	//		RID spectrum_texture;
	//
	//		//	RID half_texture[2];
	//		//
	//		//	// raster only
	//		//	RID base_fb;
	//		//	RID secondary_fb; // with weights
	//		//	RID half_fb[2]; // with weights
	//		//	RID base_weight_fb;
	//		//	RID weight_texture[4];
	//	};
	class ChromaticAberrationBuffers : public RenderBufferCustomDataRD {
		GDCLASS(ChromaticAberrationBuffers, RenderBufferCustomDataRD);

	private:
		bool prefer_raster_effects;

	public:
		RID spectrum;
		RID refraction;

		virtual void configure(RenderSceneBuffersRD *p_render_buffers) override;
		virtual void free_data() override;

		void set_prefer_raster_effects(bool p_prefer_raster_effects);
	};

	//	Ref<ChromaticAberrationBuffers> get_buffers(Ref<RenderSceneBuffersRD> p_render_buffers);
	//	void luminance_reduction(RID p_source_texture, const Size2i p_source_size, Ref<LuminanceBuffers> p_luminance_buffers, float p_min_luminance, float p_max_luminance, float p_adjust, bool p_set = false);

	//	class CACustomBuffers : public RenderBufferCustomDataRD {
	//		GDCLASS(CACustomBuffers, RenderBufferCustomDataRD);
	//
	//	private:
	//		bool prefer_raster_effects;
	//
	//	public:
	//		Vector<RID> reduce;
	//		RID current;
	//		virtual void configure(RenderSceneBuffersRD *p_render_buffers) override;
	//		// virtual void free_data() override;
	//		void set_prefer_raster_effects(bool p_prefer_raster_effects);
	//	};
	// Ref<CACustomBuffers> get_ca_buffers(Ref<RenderSceneBuffersRD> p_render_buffers);
	// RID get_current_ca_buffer(Ref<RenderSceneBuffersRD> p_render_buffers);

	// BokehDOF(bool p_prefer_raster_effects);
	ChromaticAberration(bool p_prefer_raster_effects);
	~ChromaticAberration();

	// void Luminance::luminance_reduction(RID p_source_texture, const Size2i p_source_size, Ref<LuminanceBuffers> p_luminance_buffers, float p_min_luminance, float p_max_luminance, float p_adjust, bool p_set) {
	void chromatic_aberration_process(RID p_source_texture, RID p_second_texture, const Size2i p_source_size, Ref<RenderSceneBuffersRD> p_render_buffers, Ref<ChromaticAberrationBuffers> p_buffers, RID p_camera_attributes);
	Ref<ChromaticAberration::ChromaticAberrationBuffers> get_buffers(Ref<RenderSceneBuffersRD> p_render_buffers);

	//	void initialize_spectrum_texture(const ChromaticAberrationBuffers &p_buffers);
	// void initialize_spectrum_texture(Ref<ChromaticAberrationBuffers> p_buffers);
	//	void initialize_spectrum_texture(RID p_spectrum);
	//void bokeh_dof_raster(const BokehBuffers &p_buffers, RID p_camera_attributes, float p_cam_znear, float p_cam_zfar, bool p_cam_orthogonal);

	void initialize_spectrum_texture(RID p_spectrum);
	void update_refraction_texture(RID p_refraction, RID p_camera_attributes, Size2i p_size);
};
} // namespace RendererRD
#endif // CHROMATIC_ABERRATION_RD_H