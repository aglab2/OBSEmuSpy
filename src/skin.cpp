#include "skin.h"

#include "tinyxml2.h"

#include <filesystem>
#include <stdexcept>

template<typename T>
T static load(tinyxml2::XMLElement &elem, const char *name);

template<> static const char *load(tinyxml2::XMLElement &elem, const char *name)
{
	auto attr = elem.FindAttribute(name);
	if (!attr)
		return nullptr;

	return attr->Value();
}

template<>
static std::optional<std::string> load(tinyxml2::XMLElement &elem,
				       const char *name)
{
	if (auto val = load<const char *>(elem, name)) {
		return std::string{val};
	} else {
		return std::nullopt;
	}
}

template<> static std::string load(tinyxml2::XMLElement &elem, const char *name)
{
	if (auto val = load<const char *>(elem, name)) {
		return std::string{val};
	} else {
		throw std::runtime_error("load failed");
	}
}

template<> static int load(tinyxml2::XMLElement &elem, const char *name)
{
	if (auto val = load<const char *>(elem, name)) {
		return std::atoi(val);
	} else {
		return 0;
	}
}

std::vector<SkinBackgroundDesc> loadBackgrounds(const char *_path)
{
	std::filesystem::path path{_path};
	auto skinPath = path / "skin.xml";

	tinyxml2::XMLDocument doc;
	doc.LoadFile(skinPath.u8string().c_str());

	if (doc.Error())
		throw std::logic_error(doc.ErrorStr());

	auto skinElement = doc.FirstChildElement("skin");
	if (!skinElement)
		throw std::logic_error("bad xml");

	std::vector<SkinBackgroundDesc> backgrounds;
	for (auto bgElement = skinElement->FirstChildElement("background");
	     bgElement != nullptr;
	     bgElement = bgElement->NextSiblingElement("background"))
		try {
			auto name = load<std::string>(*bgElement, "name");
			auto imagePath =
				path / load<std::string>(*bgElement, "image");
			backgrounds.emplace_back(imagePath.string(), name);
		} catch (...) {
		}

	return backgrounds;
}

Skin::Button::Name Skin::Button::toName(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(),
		       [](unsigned char c) { return std::tolower(c); });

	if (str == "a")
		return Name::A;
	if (str == "b")
		return Name::B;
	if (str == "z")
		return Name::Z;
	if (str == "start")
		return Name::Start;
	if (str == "cup")
		return Name::CUp;
	if (str == "cdown")
		return Name::CDown;
	if (str == "cleft")
		return Name::CLeft;
	if (str == "cright")
		return Name::CRight;
	if (str == "up")
		return Name::Up;
	if (str == "down")
		return Name::Down;
	if (str == "left")
		return Name::Left;
	if (str == "right")
		return Name::Right;

	throw new std::runtime_error("bad name");
}

Skin::Stick::Name Skin::Stick::toName(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(),
		       [](unsigned char c) { return std::tolower(c); });

	if (str == "stick_x")
		return Name::X;
	if (str == "stick_y")
		return Name::Y;

	throw new std::runtime_error("bad name");
}

template<> Skin::Button::Name load(tinyxml2::XMLElement &elem, const char *name)
{
	return Skin::Button::toName(load<std::string>(elem, name));
}

template<> Skin::Stick::Name load(tinyxml2::XMLElement &elem, const char *name)
{
	return Skin::Stick::toName(load<std::string>(elem, name));
}

Skin::Skin(const char *_path)
{
	std::filesystem::path path{_path};
	auto skinPath = path / "skin.xml";

	tinyxml2::XMLDocument doc;
	doc.LoadFile(skinPath.u8string().c_str());

	if (doc.Error())
		throw std::logic_error(doc.ErrorStr());

	auto skinElement = doc.FirstChildElement("skin");
	if (!skinElement)
		throw std::logic_error("bad xml");

	for (auto buttonElem = skinElement->FirstChildElement("button");
	     buttonElem != nullptr;
	     buttonElem = buttonElem->NextSiblingElement("button"))
		try {
			auto name = load<Button::Name>(*buttonElem, "name");
			auto imagePath =
				path / load<std::string>(*buttonElem, "image");
			auto x = load<int>(*buttonElem, "x");
			auto y = load<int>(*buttonElem, "y");
			auto w = load<int>(*buttonElem, "width");
			auto h = load<int>(*buttonElem, "height");

			buttons_.emplace_back(name, imagePath.string(),
					      Coordinates{x, y},
					      Coordinates{w, h});
		} catch (...) {
		}

	for (auto stickElem = skinElement->FirstChildElement("stick");
	     stickElem != nullptr;
	     stickElem = stickElem->NextSiblingElement("stick"))
		try {
			auto xname = load<Stick::Name>(*stickElem, "xname");
			auto yname = load<Stick::Name>(*stickElem, "yname");
			auto imageRelPath =
				load<std::string>(*stickElem, "image");
			auto imagePath = path / imageRelPath;
			auto x = load<int>(*stickElem, "x");
			auto y = load<int>(*stickElem, "y");
			auto w = load<int>(*stickElem, "width");
			auto h = load<int>(*stickElem, "height");
			auto xrange = load<int>(*stickElem, "xrange");
			auto yrange = load<int>(*stickElem, "yrange");

			sticks_.emplace_back(xname, yname, imagePath.string(),
					     Coordinates{x, y},
					     Coordinates{w, h},
					     Coordinates{xrange, yrange});
		} catch (...) {
		}
}

void Skin::render(Input)
{
	for (const auto &button : buttons_) {
		auto &image = button.image;
		obs_source_draw(image.texture(), button.pos.x, button.pos.y,
				button.size.x ? button.size.x : image.cx(),
				button.size.y ? button.size.y : image.cy(),
				false);
	}

	for (const auto &stick : sticks_) {
		auto &image = stick.image;
		obs_source_draw(image.texture(), stick.pos.x, stick.pos.y,
				stick.size.x ? stick.size.x : image.cx(),
				stick.size.y ? stick.size.y : image.cy(),
				false);
	}
}
