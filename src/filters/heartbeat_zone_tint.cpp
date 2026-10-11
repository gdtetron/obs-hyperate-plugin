#include "filters/heartbeat_zone_tint.hpp"
#include "heart_rate/heart_rate_state.hpp"
#include "util/log.hpp"
#include <algorithm>

namespace {

enum TintMode {
    TintModeMultiply = 0,
    TintModeReplace = 1,
};

struct HeartbeatZoneTintFilter {
    obs_source_t *source = nullptr;
    gs_effect_t *effect = nullptr;
    gs_eparam_t *tint_param = nullptr;
    gs_eparam_t *tint_strength_param = nullptr;
    gs_eparam_t *replace_mode_param = nullptr;

    bool use_smoothed = true;
    int tint_mode = TintModeMultiply;
    double active_bpm = 100.0;
    double high_bpm = 130.0;
    double peak_bpm = 160.0;
    uint32_t resting_color = 0xFF38D878;
    uint32_t active_color = 0xFF47A8FF;
    uint32_t high_color = 0xFF00D7FF;
    uint32_t peak_color = 0xFF3B3BFF;
    double tint_strength = 1.0;

    bool test_mode = false;
    double test_bpm = 120.0;
};

const char *zone_tint_get_name(void *)
{
    return obs_module_text("Filter.ZoneTint.Name");
}

void zone_tint_update(void *data, obs_data_t *settings)
{
    auto *filter = static_cast<HeartbeatZoneTintFilter *>(data);
    filter->use_smoothed = obs_data_get_bool(settings, "use_smoothed");
    filter->tint_mode = static_cast<int>(obs_data_get_int(settings, "tint_mode"));
    filter->active_bpm = obs_data_get_double(settings, "active_bpm");
    filter->high_bpm = obs_data_get_double(settings, "high_bpm");
    filter->peak_bpm = obs_data_get_double(settings, "peak_bpm");
    filter->resting_color = (uint32_t)obs_data_get_int(settings, "resting_color");
    filter->active_color = (uint32_t)obs_data_get_int(settings, "active_color");
    filter->high_color = (uint32_t)obs_data_get_int(settings, "high_color");
    filter->peak_color = (uint32_t)obs_data_get_int(settings, "peak_color");
    filter->tint_strength = std::clamp(obs_data_get_double(settings, "tint_strength"), 0.0, 1.0);
    filter->test_mode = obs_data_get_bool(settings, "test_mode");
    filter->test_bpm = obs_data_get_double(settings, "test_bpm");
}

void *zone_tint_create(obs_data_t *settings, obs_source_t *source)
{
    auto *filter = new HeartbeatZoneTintFilter;
    filter->source = source;

    char *effect_path = obs_module_file("effects/heartbeat_zone_tint.effect");
    obs_enter_graphics();
    filter->effect = gs_effect_create_from_file(effect_path, nullptr);
    obs_leave_graphics();
    bfree(effect_path);

    if (!filter->effect) {
        HYPERATE_LOG(LOG_ERROR, "Could not load heartbeat zone tint effect");
    } else {
        filter->tint_param = gs_effect_get_param_by_name(filter->effect, "tint");
        filter->tint_strength_param = gs_effect_get_param_by_name(filter->effect, "tint_strength");
        filter->replace_mode_param = gs_effect_get_param_by_name(filter->effect, "replace_mode");
    }

    zone_tint_update(filter, settings);
    return filter;
}

void zone_tint_destroy(void *data)
{
    auto *filter = static_cast<HeartbeatZoneTintFilter *>(data);
    if (filter->effect) {
        obs_enter_graphics();
        gs_effect_destroy(filter->effect);
        obs_leave_graphics();
    }
    delete filter;
}

bool test_mode_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
    obs_property_set_visible(obs_properties_get(props, "test_bpm"), obs_data_get_bool(settings, "test_mode"));
    return true;
}

obs_properties_t *zone_tint_properties(void *)
{
    obs_properties_t *props = obs_properties_create();

    obs_properties_add_bool(props, "use_smoothed", obs_module_text("Filter.ZoneTint.UseSmoothed"));

    obs_property_t *mode = obs_properties_add_list(props, "tint_mode", obs_module_text("Filter.ZoneTint.TintMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode, obs_module_text("Filter.ZoneTint.TintMode.Multiply"), TintModeMultiply);
    obs_property_list_add_int(mode, obs_module_text("Filter.ZoneTint.TintMode.Replace"), TintModeReplace);

    obs_properties_add_float(props, "active_bpm", obs_module_text("Filter.ZoneTint.ActiveBpm"), 1.0, 300.0, 1.0);
    obs_properties_add_float(props, "high_bpm", obs_module_text("Filter.ZoneTint.HighBpm"), 1.0, 300.0, 1.0);
    obs_properties_add_float(props, "peak_bpm", obs_module_text("Filter.ZoneTint.PeakBpm"), 1.0, 300.0, 1.0);

    obs_properties_add_color_alpha(props, "resting_color", obs_module_text("Filter.ZoneTint.RestingColor"));
    obs_properties_add_color_alpha(props, "active_color", obs_module_text("Filter.ZoneTint.ActiveColor"));
    obs_properties_add_color_alpha(props, "high_color", obs_module_text("Filter.ZoneTint.HighColor"));
    obs_properties_add_color_alpha(props, "peak_color", obs_module_text("Filter.ZoneTint.PeakColor"));

    obs_properties_add_float_slider(props, "tint_strength", obs_module_text("Filter.ZoneTint.TintStrength"), 0.0, 1.0, 0.01);

    obs_property_t *test_mode = obs_properties_add_bool(props, "test_mode", obs_module_text("Filter.ZoneTint.TestMode"));
    obs_property_set_modified_callback(test_mode, test_mode_changed);
    obs_properties_add_float_slider(props, "test_bpm", obs_module_text("Filter.ZoneTint.TestBpm"), 40.0, 220.0, 1.0);

    return props;
}

void zone_tint_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, "use_smoothed", true);
    obs_data_set_default_int(settings, "tint_mode", TintModeMultiply);
    obs_data_set_default_double(settings, "active_bpm", 100.0);
    obs_data_set_default_double(settings, "high_bpm", 130.0);
    obs_data_set_default_double(settings, "peak_bpm", 160.0);
    obs_data_set_default_int(settings, "resting_color", 0xFF38D878);
    obs_data_set_default_int(settings, "active_color", 0xFF47A8FF);
    obs_data_set_default_int(settings, "high_color", 0xFF00D7FF);
    obs_data_set_default_int(settings, "peak_color", 0xFF3B3BFF);
    obs_data_set_default_double(settings, "tint_strength", 1.0);
    obs_data_set_default_bool(settings, "test_mode", false);
    obs_data_set_default_double(settings, "test_bpm", 120.0);
}

double current_bpm(const HeartbeatZoneTintFilter *filter)
{
    auto snapshot = hyperate::heart_rate_state().snapshot();
    if (snapshot.has_sample && snapshot.is_live)
        return filter->use_smoothed ? snapshot.smoothed_bpm : snapshot.raw_bpm;
    return filter->test_mode ? filter->test_bpm : 0.0;
}

uint32_t current_color(const HeartbeatZoneTintFilter *filter, double bpm)
{
    if (bpm >= filter->peak_bpm) return filter->peak_color;
    if (bpm >= filter->high_bpm) return filter->high_color;
    if (bpm >= filter->active_bpm) return filter->active_color;
    return filter->resting_color;
}

void zone_tint_video_render(void *data, gs_effect_t *)
{
    auto *filter = static_cast<HeartbeatZoneTintFilter *>(data);
    obs_source_t *target = obs_filter_get_target(filter->source);
    if (!target || !filter->effect) {
        obs_source_skip_video_filter(filter->source);
        return;
    }

    const uint32_t width = obs_source_get_base_width(target);
    const uint32_t height = obs_source_get_base_height(target);

    double bpm = current_bpm(filter);
    uint32_t color = current_color(filter, bpm);

    if (!obs_source_process_filter_begin(filter->source, GS_RGBA, OBS_NO_DIRECT_RENDERING))
        return;

    if (filter->tint_param) {
        vec4 tint;
        vec4_from_rgba(&tint, color);
        gs_effect_set_vec4(filter->tint_param, &tint);
    }
    if (filter->tint_strength_param) {
        gs_effect_set_float(filter->tint_strength_param, (float)filter->tint_strength);
    }
    if (filter->replace_mode_param) {
        gs_effect_set_bool(filter->replace_mode_param, filter->tint_mode == TintModeReplace);
    }

    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
    obs_source_process_filter_end(filter->source, filter->effect, width, height);
    gs_blend_state_pop();
}

} // namespace

obs_source_info heartbeat_zone_tint_filter_info = [] {
    obs_source_info info = {};
    info.id = "hyperate_heartbeat_zone_tint";
    info.type = OBS_SOURCE_TYPE_FILTER;
    info.output_flags = OBS_SOURCE_VIDEO;
    info.get_name = zone_tint_get_name;
    info.create = zone_tint_create;
    info.destroy = zone_tint_destroy;
    info.update = zone_tint_update;
    info.get_defaults = zone_tint_defaults;
    info.get_properties = zone_tint_properties;
    info.video_render = zone_tint_video_render;
    return info;
}();
