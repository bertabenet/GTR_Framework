#ifndef SCENE_H
#define SCENE_H

#include "framework.h"
#include "shader.h"
#include "fbo.h"
#include <string>

//forward declaration
class cJSON; 


//our namespace
namespace GTR {

	enum eEntityType {
		NONE = 0,
		PREFAB = 1,
		LIGHT = 2,
		CAMERA = 3,
		REFLECTION_PROBE = 4,
		DECALL = 5
	};

	enum eLightType {
		NOLIGHT = 0,
		POINT = 1,
		SPOT = 2,
		DIRECTIONAL = 3
	};

	class Scene;
	class Prefab;

	//represents one element of the scene (could be lights, prefabs, cameras, etc)
	class BaseEntity
	{
	public:
		Scene* scene;
		std::string name;
		eEntityType entity_type;
		Matrix44 model;
		bool visible;

		BaseEntity() { entity_type = NONE; visible = true; }
		virtual ~BaseEntity() {}
		virtual void renderInMenu();
		virtual void configure(cJSON* json) {}
	};

	//represents one prefab in the scene
	class PrefabEntity : public GTR::BaseEntity
	{
	public:
		std::string filename;
		Prefab* prefab;
		
		PrefabEntity();
		virtual void renderInMenu();
		virtual void configure(cJSON* json);
	};
	
	class LightEntity : public GTR::BaseEntity 
	{
	public:
		Vector3 color;
		float intensity;
		eLightType light_type;
		float max_distance;
		float cone_angle;
		float area_size;
		Vector3 target;

		Matrix44 viewproj_mat;
		FBO fbo;

		LightEntity();
		virtual void renderInMenu();

		void setUniforms(Shader* s);
	};

	//contains all entities of the scene
	class Scene
	{
	public:
		static Scene* instance;

		Vector3 background_color;
		Vector3 ambient_light;

		Scene();

		std::string filename;
		std::vector<BaseEntity*> entities;
		std::vector<LightEntity*> l_entities;

		void clear();
		void addEntity(BaseEntity* entity);
		void addEntityLight(LightEntity* entity);

		bool load(const char* filename);
		BaseEntity* createEntity(std::string type);
	};

};

#endif