#pragma once

#include <graphics/image-file.h>
#include <obs.h>

#include "dispatch_queue.h"

class Image {
public:
	Image() = default;
	Image(const char *path) : me_(std::make_shared<gs_image_file_t>())
	{
		gs_image_file_init(me_.get(), path);
		if (!me_->loaded)
			throw std::runtime_error("Failed to load image");

		obs_enter_graphics();
		gs_image_file_init_texture(me_.get());
		obs_leave_graphics();
	}
	~Image()
	{
		if (me_) {
			gTeardownQueue->async([me{std::move(me_)}]() mutable {
				obs_enter_graphics();
				gs_image_file_free(me.get());
				obs_leave_graphics();
			});
		}
	}

	Image(const Image &) = delete;
	Image &operator=(const Image &) = delete;

	Image(Image &&o) : me_(std::move(o.me_)) {}

	Image &operator=(Image &&o) { me_ = std::move(o.me_); }

	gs_texture_t *texture() const { return me_->texture; }
	uint32_t cx() const { return me_->cx; }
	uint32_t cy() const { return me_->cy; }

private:
	std::shared_ptr<gs_image_file_t> me_;
};
