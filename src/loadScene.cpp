#include <glm/glm.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/variant.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <iostream>
#include <algorithm>
#include <map>

#include "objects/Object3D.hpp"
#include "objects/ObjModel.hpp"
#include "constants.hpp"
#include "loadScene.hpp"

namespace fs = boost::filesystem;

namespace scl {
	using scene_attribute = boost::variant<glm::vec3, float>;

	static std::string v1 = "v1";
	static std::string v2 = "v2";
	static std::string v3 = "v3";
	static std::string amb = "amb";
	static std::string dif = "dif";
	static std::string spe = "spe";
	static std::string shi = "shi";
	static std::string fov = "fov";
	static std::string f = "f";
	static std::string a = "a";
	static std::string rad = "rad";

	bool isFloatAttribute(const std::string& fieldname)
	{
		static std::vector<std::string> names = {fov, f, a, rad, shi};
		return std::find(names.begin(), names.end(), fieldname) != names.end();
	}

	std::string
	getTrimmedLineFromFile(std::ifstream* const& file, int* const& line_number)
	{
		std::string line;
		std::getline(*file, line);
		(*line_number)++;
		return boost::trim_copy(line);
	}

	std::string getTrimmedSubstring(
		const std::string& str,
		const unsigned long& pos,
		const unsigned long& n
	)
	{
		return boost::trim_copy(str.substr(pos, n));
	}

	std::string getTrimmedSubstring(const std::string& str, const unsigned long& pos)
	{
		return boost::trim_copy(str.substr(pos));
	}

	std::runtime_error scenefileFieldError(
		const std::string& fieldname,
		const std::string& filename,
		const int& line_number,
		const std::string& bad_state
	)
	{
		return std::runtime_error(
			"Field '" + fieldname + "' " + bad_state + " (" +
			filename + " line " + std::to_string(line_number) + ")"
		);
	}

	std::runtime_error unknownFieldError(
		const std::string& fieldname,
		const std::string& filename,
		const int& line_number
	)
	{
		return scenefileFieldError(fieldname, filename, line_number, "unknown");
	}

	std::runtime_error duplicateFieldError(
		const std::string& fieldname,
		const std::string& filename,
		const int& line_number
	)
	{
		return scenefileFieldError(fieldname, filename, line_number, "already defined");
	}

	std::runtime_error missingFieldError(
		const std::string& fieldname,
		const std::string& filename,
		const int& line_number
	)
	{
		return scenefileFieldError(fieldname, filename, line_number, "missing");
	}

	std::runtime_error deformedFieldError(
		const std::string& fieldname,
		const std::string& filename,
		const int& line_number
	)
	{
		return scenefileFieldError(fieldname, filename, line_number, "deformed");
	}

	void readSceneAttributes(
		const std::vector<std::string>& fieldnames,
		const std::string& filename,
		std::map<std::string, scene_attribute>* const& scene_attributes,
		std::ifstream* const& scenefile,
		int* const& line_number
	) {
		// for holding onto data while it's being parsed
		std::string temp;

		// each will be marked true if corresponding lines are read from file
		std::vector<bool> fields_collected(fieldnames.size(), false);

		for (size_t i = 0, len = fieldnames.size(); i < len; i++) {
			temp = getTrimmedLineFromFile(scenefile, line_number);
			unsigned long colon_pos = temp.find(':');
			std::string fieldname = temp.substr(0, temp.find(':'));
			if (std::find(fieldnames.begin(), fieldnames.end(), fieldname) == fieldnames.end()) {
				throw unknownFieldError(fieldname, filename, *line_number);
			}
			if (fields_collected[i]) {
				throw duplicateFieldError(fieldname, filename, *line_number);
			}
			if (isFloatAttribute(fieldname)) {
				(*scene_attributes)[fieldname] = std::stof(getTrimmedSubstring(temp, colon_pos + 1));
			} else {
				float x, y, z;
				int matches = sscanf(
					getTrimmedSubstring(temp, colon_pos + 1).c_str(),
					"%f %f %f",
					&x, &y, &z
				);
				if (matches != 3) {
					throw deformedFieldError(fieldname, filename, *line_number);
				}
				(*scene_attributes)[fieldname] = glm::vec3(x, y, z);
			}
			fields_collected[i] = true;
		}
		for (size_t j = 0, len = fieldnames.size(); j < len; j++) {
			if (!fields_collected[j]) {
				throw missingFieldError(fieldnames[j], filename, *line_number);
			}
		}
	};
}

void loadScene(const std::string& filename, std::vector<Object3D*>* const& scene_objects)
{
	try {
		int line_number = 0;

		std::ifstream scenefile(filename);

		std::string first_line = scl::getTrimmedLineFromFile(&scenefile, &line_number);
		int entity_count = std::stoi(first_line);

		// read in the number of entities specified at the top of the file
		for (int i = entity_count; i--; ) {
			std::string entity_type = scl::getTrimmedLineFromFile(&scenefile, &line_number);

			// for holding onto data while it's being parsed
			std::string temp;

			// different behavior depending on entity type
			if (entity_type == "camera") {
				for (int j = 0; j < 4; j++) {
					std::getline(scenefile, temp);
					line_number++;
				}
			} else if (entity_type == "light") {
				for (int j = 0; j < 2; j++) {
					std::getline(scenefile, temp);
					line_number++;
				}
			} else if (entity_type == "sphere") {
				for (int j = 0; j < 6; j++) {
					std::getline(scenefile, temp);
					line_number++;
				}
			} else if (entity_type == "plane") {
				for (int j = 0; j < 6; j++) {
					std::getline(scenefile, temp);
					line_number++;
				}
			} else if (entity_type == "triangle") {
				static std::vector<std::string> fieldnames = {
					scl::v1, scl::v2, scl::v3, scl::amb, scl::dif, scl::spe, scl::shi
				};
				std::map<std::string, scl::scene_attribute> scene_attributes;
				scl::readSceneAttributes(
					fieldnames,
					filename,
					&scene_attributes,
					&scenefile,
					&line_number
				);
				scene_objects->push_back(
					new Triangle(
						boost::get<glm::vec3>(scene_attributes[scl::v1]),
						boost::get<glm::vec3>(scene_attributes[scl::v2]),
						boost::get<glm::vec3>(scene_attributes[scl::v3]),
						boost::get<glm::vec3>(scene_attributes[scl::amb]),
						boost::get<glm::vec3>(scene_attributes[scl::dif]),
						boost::get<glm::vec3>(scene_attributes[scl::spe]),
						boost::get<float>(scene_attributes[scl::shi])
					)
				);
			} else if (entity_type == "model") {
				// this is always on the line after 'model'
				std::string obj_filename = scl::getTrimmedLineFromFile(&scenefile, &line_number);
				// get rid of quotation marks
				if (obj_filename[0] == '"' && obj_filename[obj_filename.length() - 1] == '"') {
					obj_filename = obj_filename.substr(1, obj_filename.length() - 2);
				}

				static std::vector<std::string> fieldnames = {
					scl::amb, scl::dif, scl::spe, scl::shi
				};
				std::map<std::string, scl::scene_attribute> scene_attributes;
				scl::readSceneAttributes(
					fieldnames,
					filename,
					&scene_attributes,
					&scenefile,
					&line_number
				);
				scene_objects->push_back(
					new ObjModel(
						(models_dir / fs::path(obj_filename)).string(),
						boost::get<glm::vec3>(scene_attributes[scl::amb]),
						boost::get<glm::vec3>(scene_attributes[scl::dif]),
						boost::get<glm::vec3>(scene_attributes[scl::spe]),
						boost::get<float>(scene_attributes[scl::shi])
					)
				);
			} else {
				throw std::runtime_error("Unknown entity type '" + entity_type + "'.");
			}
		}
	} catch(error_t e) {
		std::cerr << "Scene loading failed!" << std::endl;
		throw e;
	}
}
