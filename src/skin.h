#pragma once

#include <graphics/image-file.h>
#include <obs.h>

#include "input.h"
#include "image.h"

#include <map>
#include <optional>
#include <vector>
#include <string>

struct SkinBackgroundDesc {
	SkinBackgroundDesc(std::string _path, std::string _name)
		: path(std::move(_path)), name(std::move(_name))
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

		Button(Name _name, const std::string &_path, Coordinates _pos,
		       Coordinates _size)
			: name(_name),
			  image(_path.c_str()),
			  pos(_pos),
			  size(_size)
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

		Stick(Name _nameX, Name _nameY, const std::string &_path,
		      Coordinates _pos, Coordinates _size, Coordinates _range)
			: nameX(_nameX),
			  nameY(_nameY),
			  image(_path.c_str()),
			  pos(_pos),
			  size(_size),
			  range(_range)
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

	std::map<char, Image> numbers_;
};
