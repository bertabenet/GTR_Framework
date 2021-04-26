#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "scene.h"
#include "extra/hdre.h"

#include <algorithm>


using namespace GTR;

Renderer::Renderer() 
{
	render_mode = eRenderMode::SHOW_TEXTURE;
}


bool compareRenderCall(sRenderCall& a, sRenderCall& b)
{
	if (a.node->material->alpha_mode == b.node->material->alpha_mode) {
		//return a.node->material->_zMax < b.node->material->_zMax;

		Vector3 center_a = transformBoundingBox(a.node->getGlobalMatrix(false) * a.prefab_model, a.node->mesh->box).center;
		Vector3 center_b = transformBoundingBox(b.node->getGlobalMatrix(false) * b.prefab_model, b.node->mesh->box).center;
		float dist_a = a.camera->eye.distance(center_a);
		float dist_b = b.camera->eye.distance(center_b);

		return dist_a < dist_b;
	}
	// true -> first argument should go before
	return a.node->material->alpha_mode <= b.node->material->alpha_mode;
}

void Renderer::renderCallNum(GTR::Node* node, Camera* camera, Matrix44 model, Prefab* prefab) {

	if (node->mesh && node->material) rendercall_v.push_back(sRenderCall{ model, node, camera, prefab });
	rendercall_all.push_back(sRenderCall{ model, node, camera, prefab });

	for (int i = 0; i < node->children.size(); ++i)
		renderCallNum(node->children[i], camera, model, prefab);
}


void Renderer::renderCall(GTR::Scene* scene, Camera* camera) {

	rendercall_v.clear();

	for (int i = 0; i < scene->entities.size(); ++i)
	{
		BaseEntity* ent = scene->entities[i];
		if (!ent->visible)
			continue;

		if (ent->entity_type == PREFAB)
		{
			PrefabEntity* pent = (GTR::PrefabEntity*)ent;
			if (pent->prefab) {
				renderCallNum(&pent->prefab->root, camera, ent->model, pent->prefab);
			}
		}
	}

	std::sort(rendercall_v.begin(), rendercall_v.end(), compareRenderCall);
}


void Renderer::renderScene(GTR::Scene* scene, Camera* camera)
{
	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	checkGLErrors();

	bool rendercall = true;
	if (rendercall) {
		renderCall(scene, camera);
		for (std::vector<sRenderCall>::iterator it = rendercall_v.begin(); it != rendercall_v.end(); ++it) {
			//renderPrefab((*it).prefab_model, (*it).prefab, (*it).camera);
			renderNode((*it).prefab_model, (*it).node, camera);
		}
	}
	else {
		//render entities
		for (int i = 0; i < scene->entities.size(); ++i)
		{
			BaseEntity* ent = scene->entities[i];
			if (!ent->visible)
				continue;

			//is a prefab!
			if (ent->entity_type == PREFAB)
			{
				PrefabEntity* pent = (GTR::PrefabEntity*)ent;
				if (pent->prefab)
					renderPrefab(ent->model, pent->prefab, camera);
			}
		}
	}
}

//renders all the prefab
void Renderer::renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera)
{
	assert(prefab && "PREFAB IS NULL");
	//assign the model to the root node
	renderNode(model, &prefab->root, camera);
}

//renders a node of the prefab and its children
void Renderer::renderNode(const Matrix44& prefab_model, GTR::Node* node, Camera* camera)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model,node->mesh->box);
		
		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) )
		{
			//render node mesh
			renderMeshWithMaterial( node_model, node->mesh, node->material, camera );
			//node->mesh->renderBounding(node_model, true);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode(prefab_model, node->children[i], camera);
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;
	GTR::Scene* scene = GTR::Scene::instance;

	texture = material->color_texture.texture;
	//texture = material->emissive_texture;
	//texture = material->metallic_roughness_texture.texture; // occlusion_metallic_roughness
	//texture = material->normal_texture;
	//texture = material->occlusion_texture.texture;
	if (texture == NULL)
		texture = Texture::getWhiteTexture(); //a 1x1 white texture

	Texture* metallic_rougness_texture = material->metallic_roughness_texture.texture;
	if(!metallic_rougness_texture) metallic_rougness_texture = Texture::getWhiteTexture();
	Texture* emissive_texture = material->emissive_texture.texture;
	if (!emissive_texture) emissive_texture = Texture::getWhiteTexture();

	//select the	
	if (material->alpha_mode == GTR::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if(material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
    assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	switch (render_mode) {
		case SHOW_NORMAL: shader = Shader::Get("normal"); break;
		case SHOW_UVS: shader = Shader::Get("uvs"); break;
		case SHOW_TEXTURE: shader = Shader::Get("texture"); break;
		case SHOW_AO: shader = Shader::Get("occlusion"); break;
		case DEFAULT: shader = Shader::Get("light_singlepass"); break;
	}
	

    assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model );
	float t = getTime();
	shader->setUniform("u_time", t );

	shader->setUniform("u_color", material->color);
	shader->setUniform("u_emissive_factor", material->emissive_factor);
	if(texture) shader->setUniform("u_texture", texture, 0);
	if (metallic_rougness_texture) shader->setUniform("u_metallic_roughness_texture", metallic_rougness_texture, 1);
	if (emissive_texture) shader->setUniform("u_emissive_texture", emissive_texture, 2);

	shader->setUniform("u_ambient_light", scene->ambient_light);
	shader->setUniform("u_test", Vector3(-33.7, 36.6, -11.8)); //lent->model.rotateVector(Vector3(0,1,0))
	shader->setUniform("u_testcolor", Vector3(0.0, 0.0, 0.0));

	/*for (int i = 0; i < scene->entities.size(); ++i) {
		BaseEntity* ent = scene->entities[i];
		if (!ent->visible)
			continue;

		//is a light!
		if (ent->entity_type == LIGHT)
		{
			LightEntity* lent = (GTR::LightEntity*)ent;
			
		}
	}*/

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::eAlphaMode::MASK ? material->alpha_cutoff : 0);

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}


Texture* GTR::CubemapFromHDRE(const char* filename)
{
	HDRE* hdre = new HDRE();
	if (!hdre->load(filename))
	{
		delete hdre;
		return NULL;
	}

	/*
	Texture* texture = new Texture();
	texture->createCubemap(hdre->width, hdre->height, (Uint8**)hdre->getFaces(0), hdre->header.numChannels == 3 ? GL_RGB : GL_RGBA, GL_FLOAT );
	for(int i = 1; i < 6; ++i)
		texture->uploadCubemap(texture->format, texture->type, false, (Uint8**)hdre->getFaces(i), GL_RGBA32F, i);
	return texture;
	*/
	return NULL;
}