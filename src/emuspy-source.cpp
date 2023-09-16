#include "emuspy-source.h"

#include <graphics/graphics.h>

static const char *emuspy_source_getname(void *data)
{
	UNUSED_PARAMETER(data);
	return "EmuSpy";
}

static obs_properties_t *emuspy_source_getproperties(void *data)
{
	return reinterpret_cast<EmuSpy *>(data)->getProperties();
}

static void emuspy_source_getdefaults(obs_data_t *settings) {}

static void *emuspy_source_create(obs_data_t *settings, obs_source_t *source)
{
	return new EmuSpy(settings);
}

static void emuspy_source_update(void *data, obs_data_t *settings)
{
	return reinterpret_cast<EmuSpy *>(data)->update(settings);
}

static void emuspy_source_destroy(void *data)
{
	delete reinterpret_cast<EmuSpy *>(data);
}

static void emuspy_video_render(void *data, gs_effect_t *effect)
{
	return reinterpret_cast<EmuSpy *>(data)->videoRender(effect);
}

static uint32_t emuspy_get_width(void *data)
{
	return reinterpret_cast<EmuSpy *>(data)->width();
}

static uint32_t emuspy_get_height(void *data)
{
	return reinterpret_cast<EmuSpy *>(data)->height();
}

static void emuspy_activate(void *data)
{
	return reinterpret_cast<EmuSpy *>(data)->activate();
}

static void emuspy_deactivate(void *data)
{
	return reinterpret_cast<EmuSpy *>(data)->deactivate();
}

obs_source_info EmuSpy::makeOBSSourceInfo()
{
	obs_source_info src{};
	src.id = "emuspy_source";
	src.type = OBS_SOURCE_TYPE_INPUT;
	src.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE |
			   OBS_SOURCE_CUSTOM_DRAW;

	src.get_name = emuspy_source_getname;
	src.get_properties = emuspy_source_getproperties;
	src.get_defaults = emuspy_source_getdefaults;
	src.update = emuspy_source_update;
	src.create = emuspy_source_create;
	src.destroy = emuspy_source_destroy;
	src.video_render = emuspy_video_render;
	src.get_width = emuspy_get_width;
	src.get_height = emuspy_get_height;
	src.activate = emuspy_activate;
	src.deactivate = emuspy_deactivate;

	return src;
}

EmuSpy::EmuSpy(obs_data_t *settings)
{
	update(settings);
}

bool EmuSpy::skinSelectedProxy(void *priv, obs_properties_t *props,
			       obs_property_t *p, obs_data_t *settings)
{
	return reinterpret_cast<EmuSpy *>(priv)->skinSelected(props, p,
							      settings);
}

bool EmuSpy::bgSelectedProxy(void *priv, obs_properties_t *props,
			     obs_property_t *p, obs_data_t *settings)
{
	return reinterpret_cast<EmuSpy *>(priv)->bgSelected(props, p, settings);
}

bool EmuSpy::skinSelected(obs_properties_t *props, obs_property_t *p,
			  obs_data_t *settings)
try {
	const char *path = obs_data_get_string(settings, "skin");
	if (!path)
		return false;

	std::atomic_store(&skin_, std::make_shared<Skin>(path));

	auto bgs = loadBackgrounds(path);
	obs_property_t *prop = obs_properties_get(props, "bg");
	obs_property_list_clear(prop);
	for (const auto &bg : bgs) {
		obs_property_list_add_string(prop, bg.name.c_str(),
					     bg.path.c_str());
	}

	obs_property_modified(prop, settings);
	return true;
} catch (...) {
	return false;
}

bool EmuSpy::bgSelected(obs_properties_t *props, obs_property_t *p,
			obs_data_t *settings)

try {
	try {
		if (const char *path = obs_data_get_string(settings, "bg"))
			std::atomic_store(&bg_, std::make_shared<Image>(path));
	} catch (...) {
	}
	return true;
} catch (...) {
	return false;
}

obs_properties_t *EmuSpy::getProperties()
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

	auto skinProp = obs_properties_add_path(
		props, "skin", "Skin", OBS_PATH_DIRECTORY, nullptr, nullptr);
	auto bgProp = obs_properties_add_list(props, "bg", "Background",
					      OBS_COMBO_TYPE_LIST,
					      OBS_COMBO_FORMAT_STRING);

	obs_property_set_modified_callback2(skinProp, skinSelectedProxy, this);
	obs_property_set_modified_callback2(bgProp, bgSelectedProxy, this);

	return props;
}

void EmuSpy::videoRender(gs_effect_t *effect)
try {
	auto effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_blend_state_push();
	gs_reset_blend_state();
	while (gs_effect_loop(effect, "Draw")) {
		if (auto bg = std::atomic_load(&bg_)) {
			obs_source_draw(bg->texture(), 0, 0, bg->cx(), bg->cy(),
					false);
		}

		if (auto skin = std::atomic_load(&skin_)) {
			Input input;
			*(int32_t *)&input = emulator_->getInputs();
			skin->render(input);
		}
	}

	gs_blend_state_pop();
} catch (...) {
}

void EmuSpy::update(obs_data_t *settings)
{
	try {
		if (const char *path = obs_data_get_string(settings, "skin"))
			std::atomic_store(&skin_, std::make_shared<Skin>(path));
	} catch (...) {
	}

	try {
		if (const char *path = obs_data_get_string(settings, "bg"))
			std::atomic_store(&bg_, std::make_shared<Image>(path));
	} catch (...) {
	}
}

uint32_t EmuSpy::width()
try {
	auto bg = std::atomic_load(&bg_);
	return bg ? bg->cx() : 0;
} catch (...) {
	return 0;
}

uint32_t EmuSpy::height()
try {
	auto bg = std::atomic_load(&bg_);
	return bg ? bg->cy() : 0;
} catch (...) {
	return 0;
}

void EmuSpy::activate()
try {
	emulator_ = std::make_shared<Emulator>();
} catch (...) {
}

void EmuSpy::deactivate()
try {
	gTeardownQueue->async([emu{std::move(emulator_)}]() {});
} catch (...) {
}