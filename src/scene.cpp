#include "scene.h"
#include "utils.h"

#include "prefab.h"
#include "extra/cJSON.h"

GTR::Scene* GTR::Scene::instance = NULL;

GTR::Scene::Scene()
{
	instance = this;
}

void GTR::Scene::clear()
{
	for (int i = 0; i < entities.size(); ++i)
	{
		BaseEntity* ent = entities[i];
		delete ent;
	}
	entities.resize(0);
}


void GTR::Scene::addEntity(BaseEntity* entity)
{
	entities.push_back(entity); entity->scene = this;
}

bool GTR::Scene::load(const char* filename)
{
	std::string content;

	this->filename = filename;
	std::cout << " + Reading scene JSON: " << filename << "..." << std::endl;

	if (!readFile(filename, content))
	{
		std::cout << "- ERROR: Scene file not found: " << filename << std::endl;
		return false;
	}

	//parse json string 
	cJSON* json = cJSON_Parse(content.c_str());
	if (!json)
	{
		std::cout << "ERROR: Scene JSON has errors: " << filename << std::endl;
		return false;
	}

	//read global properties
	background_color = readJSONVector3(json, "background_color", background_color);
	ambient_light = readJSONVector3(json, "ambient_light", ambient_light );

	//entities
	cJSON* entities_json = cJSON_GetObjectItemCaseSensitive(json, "entities");
	cJSON* entity_json;
	cJSON_ArrayForEach(entity_json, entities_json)
	{
		std::string type_str = cJSON_GetObjectItem(entity_json, "type")->valuestring;
		BaseEntity* ent = createEntity(type_str);
		if (!ent)
		{
			std::cout << " - ENTITY TYPE UNKNOWN: " << type_str << std::endl;
			//continue;
			ent = new BaseEntity();
		}

		addEntity(ent);

		if (cJSON_GetObjectItem(entity_json, "name"))
		{
			ent->name = cJSON_GetObjectItem(entity_json, "name")->valuestring;
			stdlog(std::string(" + entity: ") + ent->name);
		}

		//read transform
		if (cJSON_GetObjectItem(entity_json, "position"))
		{
			ent->model.setIdentity();
			Vector3 position = readJSONVector3(entity_json, "position", Vector3());
			ent->model.translate(position.x, position.y, position.z);
		}

		if (cJSON_GetObjectItem(entity_json, "angle"))
		{
			float angle = cJSON_GetObjectItem(entity_json, "angle")->valuedouble;
			ent->model.rotate(angle * DEG2RAD, Vector3(0, 1, 0));
		}

		if (cJSON_GetObjectItem(entity_json, "rotation"))
		{
			Vector4 rotation = readJSONVector4(entity_json, "rotation");
			Quaternion q(rotation.x, rotation.y, rotation.z, rotation.w);
			Matrix44 R;
			q.toMatrix(R);
			ent->model = R * ent->model;
		}

		if (cJSON_GetObjectItem(entity_json, "scale"))
		{
			Vector3 scale = readJSONVector3(entity_json, "scale", Vector3(1, 1, 1));
			ent->model.scale(scale.x, scale.y, scale.z);
		}

		if (cJSON_GetObjectItem(entity_json, "target"))
		{
			LightEntity* lent = (LightEntity*)ent;
			Vector3 target = readJSONVector3(entity_json, "target", Vector3());
			lent->target = target;
		}

		if (cJSON_GetObjectItem(entity_json, "color"))
		{
			LightEntity* lent = (LightEntity*)ent;
			Vector3 color = readJSONVector3(entity_json, "color", Vector3());
			lent->color = color;
		}

		if (cJSON_GetObjectItem(entity_json, "light_type"))
		{
			LightEntity* lent = (LightEntity*)ent;
			std::string type = cJSON_GetObjectItem(entity_json, "light_type")->valuestring;
			if (type == "POINT") lent->light_type = eLightType::POINT;
			if (type == "SPOT") lent->light_type = eLightType::SPOT;
			if (type == "DIRECTIONAL") lent->light_type = eLightType::DIRECTIONAL;
		}

		if (cJSON_GetObjectItem(entity_json, "intensity"))
		{
			LightEntity* lent = (LightEntity*)ent;
			float angle = cJSON_GetObjectItem(entity_json, "intensity")->valuedouble;
			lent->intensity = angle;
		}

		if (cJSON_GetObjectItem(entity_json, "max_distance"))
		{
			LightEntity* lent = (LightEntity*)ent;
			float max_distance = cJSON_GetObjectItem(entity_json, "max_distance")->valuedouble;
			lent->max_distance = max_distance;
		}

		if (cJSON_GetObjectItem(entity_json, "cone_angle"))
		{
			LightEntity* lent = (LightEntity*)ent;
			float cone_angle = cJSON_GetObjectItem(entity_json, "cone_angle")->valuedouble;
			lent->cone_angle = cone_angle;
		}

		if (cJSON_GetObjectItem(entity_json, "area_size"))
		{
			LightEntity* lent = (LightEntity*)ent;
			float area_size = cJSON_GetObjectItem(entity_json, "area_size")->valuedouble;
			lent->area_size = area_size;
		}

		ent->configure(entity_json);
	}

	//free memory
	cJSON_Delete(json);

	return true;
}

GTR::BaseEntity* GTR::Scene::createEntity(std::string type)
{
	if (type == "PREFAB")
		return new GTR::PrefabEntity();
	else if (type == "LIGHT")
		return new GTR::LightEntity();
    return NULL;
}

void GTR::BaseEntity::renderInMenu()
{
#ifndef SKIP_IMGUI
	ImGui::Text("Name: %s", name.c_str()); // Edit 3 floats representing a color
	ImGui::Checkbox("Visible", &visible); // Edit 3 floats representing a color
	//Model edit
	ImGuiMatrix44(model, "Model");
#endif
}




GTR::PrefabEntity::PrefabEntity()
{
	entity_type = PREFAB;
	prefab = NULL;
}

void GTR::PrefabEntity::configure(cJSON* json)
{
	if (cJSON_GetObjectItem(json, "filename"))
	{
		filename = cJSON_GetObjectItem(json, "filename")->valuestring;
		prefab = GTR::Prefab::Get( (std::string("data/") + filename).c_str());
	}
}

void GTR::PrefabEntity::renderInMenu()
{
	BaseEntity::renderInMenu();

#ifndef SKIP_IMGUI
	ImGui::Text("filename: %s", filename.c_str()); // Edit 3 floats representing a color
	if (prefab && ImGui::TreeNode(prefab, "Prefab Info"))
	{
		prefab->root.renderInMenu();
		ImGui::TreePop();
	}
#endif
}


GTR::LightEntity::LightEntity()
{
	entity_type = LIGHT;	
}

void GTR::LightEntity::renderInMenu()
{
	BaseEntity::renderInMenu();

#ifndef SKIP_IMGUI
	ImGui::ColorEdit4("Color", color.v);
	ImGui::DragFloat("Intensity", &intensity, 0.01f);
	ImGui::DragFloat("Max distance", &max_distance);
	ImGui::DragFloat("Area size", &area_size, 0.01f);
	ImGui::DragFloat3("Target", target.v);
#endif
}

void GTR::LightEntity::setUniforms(Shader* s) {
	s->setUniform("u_light_type", (int)light_type);
	s->setUniform("u_light_position", model.bottomVector());
	s->setUniform("u_light_color", color);
	s->setUniform("u_direction", target);
	s->setUniform("u_spotExponent", (1 / area_size));
	s->setUniform("u_spotCosineCutoff", cos(cone_angle));
	s->setUniform("u_light_factor", intensity);
	s->setUniform("u_maxdist", max_distance);
}