/**************************************************************************/
/*  camera_attributes_storage.cpp                                         */
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

#include "camera_attributes_storage.h"

RendererCameraAttributes *RendererCameraAttributes::singleton = nullptr;
uint64_t RendererCameraAttributes::auto_exposure_counter = 2;

RendererCameraAttributes::RendererCameraAttributes() {
	singleton = this;
}

RendererCameraAttributes::~RendererCameraAttributes() {
	singleton = nullptr;
}

RID RendererCameraAttributes::camera_attributes_allocate() {
	return camera_attributes_owner.allocate_rid();
}

void RendererCameraAttributes::camera_attributes_initialize(RID p_rid) {
	camera_attributes_owner.initialize_rid(p_rid, CameraAttributes());
}

void RendererCameraAttributes::camera_attributes_free(RID p_rid) {
	camera_attributes_owner.free(p_rid);
}

void RendererCameraAttributes::camera_attributes_set_dof_blur_quality(RS::DOFBlurQuality p_quality, bool p_use_jitter) {
	dof_blur_quality = p_quality;
	dof_blur_use_jitter = p_use_jitter;
}

void RendererCameraAttributes::camera_attributes_set_dof_blur_bokeh_shape(RS::DOFBokehShape p_shape) {
	dof_blur_bokeh_shape = p_shape;
}

void RendererCameraAttributes::camera_attributes_set_dof_blur(RID p_camera_attributes, bool p_far_enable, float p_far_distance, float p_far_transition, bool p_near_enable, float p_near_distance, float p_near_transition, float p_amount) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND(!cam_attributes);
#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility" && (p_far_enable || p_near_enable)) {
		WARN_PRINT_ONCE_ED("DoF blur is only available when using the Forward+ or Mobile rendering backends.");
	}
#endif
	cam_attributes->dof_blur_far_enabled = p_far_enable;
	cam_attributes->dof_blur_far_distance = p_far_distance;
	cam_attributes->dof_blur_far_transition = p_far_transition;

	cam_attributes->dof_blur_near_enabled = p_near_enable;
	cam_attributes->dof_blur_near_distance = p_near_distance;
	cam_attributes->dof_blur_near_transition = p_near_transition;

	cam_attributes->dof_blur_amount = p_amount;
}

bool RendererCameraAttributes::camera_attributes_get_dof_far_enabled(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, false);
	return cam_attributes->dof_blur_far_enabled;
}

float RendererCameraAttributes::camera_attributes_get_dof_far_distance(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->dof_blur_far_distance;
}

float RendererCameraAttributes::camera_attributes_get_dof_far_transition(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->dof_blur_far_transition;
}

bool RendererCameraAttributes::camera_attributes_get_dof_near_enabled(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, false);
	return cam_attributes->dof_blur_near_enabled;
}

float RendererCameraAttributes::camera_attributes_get_dof_near_distance(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->dof_blur_near_distance;
}

float RendererCameraAttributes::camera_attributes_get_dof_near_transition(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->dof_blur_near_transition;
}

float RendererCameraAttributes::camera_attributes_get_dof_blur_amount(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->dof_blur_amount;
}

void RendererCameraAttributes::camera_attributes_set_exposure(RID p_camera_attributes, float p_multiplier, float p_exposure_normalization) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND(!cam_attributes);
	cam_attributes->exposure_multiplier = p_multiplier;
	cam_attributes->exposure_normalization = p_exposure_normalization;
}

float RendererCameraAttributes::camera_attributes_get_exposure_normalization_factor(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 1.0);

	return cam_attributes->exposure_multiplier * cam_attributes->exposure_normalization;
}

void RendererCameraAttributes::camera_attributes_set_auto_exposure(RID p_camera_attributes, bool p_enable, float p_min_sensitivity, float p_max_sensitivity, float p_speed, float p_scale) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND(!cam_attributes);
	if (!cam_attributes->use_auto_exposure && p_enable) {
		cam_attributes->auto_exposure_version = ++auto_exposure_counter;
	}
#ifdef DEBUG_ENABLED
	if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility" && p_enable) {
		WARN_PRINT_ONCE_ED("Auto exposure is only available when using the Forward+ or Mobile rendering backends.");
	}
#endif
	cam_attributes->use_auto_exposure = p_enable;
	cam_attributes->auto_exposure_min_sensitivity = p_min_sensitivity;
	cam_attributes->auto_exposure_max_sensitivity = p_max_sensitivity;
	cam_attributes->auto_exposure_adjust_speed = p_speed;
	cam_attributes->auto_exposure_scale = p_scale;
}

float RendererCameraAttributes::camera_attributes_get_auto_exposure_min_sensitivity(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->auto_exposure_min_sensitivity;
}

float RendererCameraAttributes::camera_attributes_get_auto_exposure_max_sensitivity(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->auto_exposure_max_sensitivity;
}

float RendererCameraAttributes::camera_attributes_get_auto_exposure_adjust_speed(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->auto_exposure_adjust_speed;
}

float RendererCameraAttributes::camera_attributes_get_auto_exposure_scale(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->auto_exposure_scale;
}

uint64_t RendererCameraAttributes::camera_attributes_get_auto_exposure_version(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0);
	return cam_attributes->auto_exposure_version;
}

void RendererCameraAttributes::camera_attributes_set_chromatic_aberration(RID p_camera_attributes, float p_angle_factor, float p_max_distance, float p_sensor_diagonal, float p_lens_center_line, float p_refract_index, float p_curvature_radius, float p_diagonal_fov, float p_apothem) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND(!cam_attributes);
#ifdef DEBUG_ENABLED
	// if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility" && (p_axial > 0.0 || p_transverse > 0.0)) {
	if (OS::get_singleton()->get_current_rendering_method() == "gl_compatibility") {
		WARN_PRINT_ONCE_ED("Chromatic aberration is only available when using the Forward+ or Mobile rendering backends.");
	}
#endif
	//	cam_attributes->chromatic_aberration_axial_amount = p_axial;
	//	cam_attributes->chromatic_aberration_transverse_amount = p_transverse;
	//	cam_attributes->chromatic_aberration_image_distance_red = p_image_distance_red;
	//	cam_attributes->chromatic_aberration_image_distance_green = p_image_distance_green;
	//	cam_attributes->chromatic_aberration_image_distance_blue = p_image_distance_blue;

	cam_attributes->chromatic_aberration_max_distance = p_max_distance;
	cam_attributes->chromatic_aberration_lens_center_line = p_lens_center_line;
	cam_attributes->chromatic_aberration_refract_index = p_refract_index;
	cam_attributes->chromatic_aberration_sensor_diagonal = p_sensor_diagonal;
	cam_attributes->chromatic_aberration_curvature_radius = p_curvature_radius;
	cam_attributes->chromatic_aberration_diagonal_fov = p_diagonal_fov;
	cam_attributes->chromatic_aberration_diagonal_fov = p_apothem;
	cam_attributes->chromatic_aberration_angle_factor = p_angle_factor;

	//	cam_attributes->chromatic_aberration_image_distance = p_half_fov;
	//	cam_attributes->chromatic_aberration_angle_factor = p_lens_radius;
}

// float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_axial_amount(RID p_camera_attributes) {
// 	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
// 	ERR_FAIL_COND_V(!cam_attributes, 0.0);
// 	return cam_attributes->chromatic_aberration_axial_amount;
// }
//
// float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_transverse_amount(RID p_camera_attributes) {
// 	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
// 	ERR_FAIL_COND_V(!cam_attributes, 0.0);
// 	return cam_attributes->chromatic_aberration_transverse_amount;
// }

//float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_image_distance_red(RID p_camera_attributes) {
//	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
//	ERR_FAIL_COND_V(!cam_attributes, 0.0);
//	return cam_attributes->chromatic_aberration_image_distance_red;
//}

//float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_image_distance_green(RID p_camera_attributes) {
//	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
//	ERR_FAIL_COND_V(!cam_attributes, 0.0);
//	return cam_attributes->chromatic_aberration_image_distance_red;
//}

//float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_image_distance_blue(RID p_camera_attributes) {
//	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
//	ERR_FAIL_COND_V(!cam_attributes, 0.0);
//	return cam_attributes->chromatic_aberration_image_distance_red;
//}

float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_max_distance(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->chromatic_aberration_max_distance;
}

float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_refract_index(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->chromatic_aberration_refract_index;
}

float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_lens_center_line(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->chromatic_aberration_lens_center_line;
}

float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_angle_factor(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->chromatic_aberration_angle_factor;
}

float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_sensor_diagonal(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->chromatic_aberration_sensor_diagonal;
}

float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_diagonal_fov(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->chromatic_aberration_diagonal_fov;
}

float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_apothem(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->chromatic_aberration_apothem;
}

float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_focal_length(RID p_camera_attributes) {
	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
	ERR_FAIL_COND_V(!cam_attributes, 0.0);
	return cam_attributes->chromatic_aberration_focal_length;
}
// float RendererCameraAttributes::camera_attributes_get_chromatic_aberration_angle_factor(RID p_camera_attributes) {
// 	CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);
// 	ERR_FAIL_COND_V(!cam_attributes, 0.0);
// 	return cam_attributes->chromatic_aberration_angle_factor;
// }