#include <obs-module.h>

#include "skin.h"

#include <atomic>
#include <memory>

class EmuSpy {
public:
	EmuSpy(obs_data_t *settings);

	static obs_source_info makeOBSSourceInfo();

	void videoRender(gs_effect_t *effect);
	void update(obs_data_t *settings);
	uint32_t width();
	uint32_t height();
	obs_properties_t *getProperties();

private:
	static bool skinSelectedProxy(void *priv, obs_properties_t *props,
				      obs_property_t *p, obs_data_t *settings);
	static bool bgSelectedProxy(void *priv, obs_properties_t *props,
				    obs_property_t *p, obs_data_t *settings);

	bool skinSelected(obs_properties_t *props, obs_property_t *p,
			  obs_data_t *settings);
	bool bgSelected(obs_properties_t *props, obs_property_t *p,
			obs_data_t *settings);

	std::atomic<Input> input_;
	std::shared_ptr<Image> bg_;
	std::shared_ptr<Skin> skin_;
};
