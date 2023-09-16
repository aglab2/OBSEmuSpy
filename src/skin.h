#pragma once

#include <graphics/image-file.h>
#include <obs.h>

#include "input.h"
#include "image.h"

#include <optional>
#include <vector>
#include <string>

struct SkinBackgroundDesc {
	SkinBackgroundDesc(std::string path, std::string name)
		: path(std::move(path)), name(std::move(name))
	{
	}

	std::string path;
	std::string name;
};

std::vector<SkinBackgroundDesc> loadBackgrounds(const char *path);

class Skin {
public:
	Skin(const char *path);

	void render(Input);

	struct Coordinates {
		int x, y;
	};

	struct Button {
		enum class Name {
			CRight,
			CLeft,
			CDown,
			CUp,
			R,
			L,
			X, // reserved...
			Y,

			Right,
			Left,
			Down,
			Up,
			Start,
			Z,
			B,
			A,
		};
		static Name toName(std::string str);

		Button(Name name, const std::string &path, Coordinates pos,
		       Coordinates size)
			: name(name), image(path.c_str()), pos(pos), size(size)
		{
		}

		Name name;
		Image image;
		Coordinates pos;
		Coordinates size;
	};

	struct Stick {
		enum class Name { X, Y };
		static Name toName(std::string str);

		Stick(Name nameX, Name nameY, const std::string &path,
		      Coordinates pos, Coordinates size, Coordinates range)
			: nameX(nameX),
			  nameY(nameY),
			  image(path.c_str()),
			  pos(pos),
			  size(size),
			  range(range)
		{
		}

		Name nameX, nameY;
		Image image;
		Coordinates pos;
		Coordinates size;
		Coordinates range;
	};

private:
	std::vector<Button> buttons_;
	std::vector<Stick> sticks_;
};
