#pragma once
#include "prefab.h"

//forward declarations
class Camera;

namespace GTR {

	enum eRenderMode{
		DEFAULT,
		SHOW_TEXTURE,
		SHOW_NORMAL,
		SHOW_AO,
		SHOW_UVS,
		SHOW_MULTI
	};

	struct sRenderCall {
		Matrix44 prefab_model;
		Node* node;
		Camera* camera;
		Prefab* prefab;
	};

	class Prefab;
	class Material;
	
	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{

	public:

		std::vector<sRenderCall> rendercall_v;

		eRenderMode render_mode;

		Renderer();

		//add here your functions
		void renderCall(GTR::Scene* scene, Camera* camera);
		void renderCallNum(GTR::Node* node, Camera* camera, Matrix44 model, Prefab* prefab);

		//renders several elements of the scene
		void renderScene(GTR::Scene* scene, Camera* camera);
	
		//to render a whole prefab (with all its nodes)
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera);

		//to render one node from the prefab and its children
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
	};

	Texture* CubemapFromHDRE(const char* filename);

};