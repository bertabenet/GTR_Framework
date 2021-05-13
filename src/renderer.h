#pragma once
#include "prefab.h"
#include "fbo.h"

//forward declarations
class Camera;

namespace GTR {

	enum eRenderMode{
		DEFAULT,
		SHOW_TEXTURE,
		SHOW_NORMAL,
		SHOW_AO,
		SHOW_UVS,
		SHOW_MULTI,
		SHOW_DEPTH,
		SHOW_GBUFFERS,
		SHOW_DEFERRED
	};

	enum ePipelineMode {
		DEFERRED,
		FORWARD
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
		ePipelineMode pipeline_mode;
		bool render_alpha;

		FBO gbuffers_fbo;
		FBO illumination_fbo;

		Renderer(GTR::Scene* scene);

		//add here your functions
		void renderCall(GTR::Scene* scene, Camera* camera);
		void renderCallNum(GTR::Node* node, Camera* camera, Matrix44 model, Prefab* prefab);

		void renderToFBO(GTR::Scene* scene, Camera* camera);

		void renderToFBOForward(GTR::Scene* scene, Camera* camera);
		void renderToFBODeferred(GTR::Scene* scene, Camera* camera);
		void renderMeshDeferred(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);

		//renders several elements of the scene
		void renderScene(GTR::Scene* scene, Camera* camera);
		void joinGbuffers(GTR::Scene* scene, Camera* camera);
		void illuminationDeferred(GTR::Scene* scene, Camera* camera);

		//to render a whole prefab (with all its nodes)
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera);

		//to render one node from the prefab and its children
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
	};

	Texture* CubemapFromHDRE(const char* filename);

};