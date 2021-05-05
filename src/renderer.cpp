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
#include "application.h"


using namespace GTR;

Renderer::Renderer(GTR::Scene* scene)
{
	render_mode = eRenderMode::SHOW_TEXTURE;
	for (int i = 0; i < scene->l_entities.size(); ++i) {
		LightEntity* lent = scene->l_entities[i];
		lent->fbo.create(Application::instance->window_width, Application::instance->window_height, 1, GL_RGB, GL_FLOAT);
	}
}

void Renderer::renderToFBO(GTR::Scene* scene, Camera* camera) {

	Camera* cam = new Camera();
	for (int i = 0; i < scene->l_entities.size(); ++i) {
		LightEntity* lent = scene->l_entities[i];
		if (lent->light_type == eLightType::SPOT) {
			cam->lookAt(lent->model.bottomVector(), lent->model.bottomVector() + lent->target, Vector3(0.f, 1.f, 0.f));
			cam->setPerspective(lent->cone_angle, Application::instance->window_width / (float)Application::instance->window_height, 1.0f, 10000.f);
		}
		else if (lent->light_type == eLightType::DIRECTIONAL) {
			cam->lookAt(lent->model.bottomVector(), Vector3(0.0f, 0.0f, 0.0f), Vector3(-1.0f, -1.0f, 0.f));
			cam->setOrthographic(-600,600,-600, 600,-600,600);			
		}
		else if (lent->light_type == eLightType::POINT) {
			continue;
		}
		lent->viewproj_mat = cam->viewprojection_matrix;
		lent->fbo.bind();
		render_alpha = false;
		renderScene(scene, cam);
		lent->fbo.unbind();
	}


	for (int i = 0; i < scene->l_entities.size(); ++i) {
		LightEntity* lent = scene->l_entities[i];
		if (lent->name == "headlight1") {
			if (render_mode == SHOW_DEPTH) {
				Shader* shader = Shader::Get("depth");
				shader->enable();
				shader->setUniform("u_camera_nearfar", Vector2(cam->near_plane, cam->far_plane));
				lent->fbo.depth_texture->toViewport(shader);
			}
		}
	}

	if (render_mode != SHOW_DEPTH) 
	{
		render_alpha = true;
		renderScene(scene, camera);
	}

	
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

	if (node->mesh && node->material) 
		if (!(render_alpha == false && node->material->alpha_mode == BLEND))
			rendercall_v.push_back(sRenderCall{ model, node, camera, prefab });

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

	if (true) {
		renderCall(scene, camera);
		for (std::vector<sRenderCall>::iterator it = rendercall_v.begin(); it != rendercall_v.end(); ++it) {
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
		BoundingBox world_bounding = transformBoundingBox(node_model, node->mesh->box);

		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
		{
			//render node mesh
			renderMeshWithMaterial(node_model, node->mesh, node->material, camera);
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
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;
	GTR::Scene* scene = GTR::Scene::instance;

	texture = material->color_texture.texture;
	if (texture == NULL)
		texture = Texture::getWhiteTexture(); //a 1x1 white texture

	bool read_normal = true;
	Texture* metallic_rougness_texture = material->metallic_roughness_texture.texture;
	if (!metallic_rougness_texture) metallic_rougness_texture = Texture::getWhiteTexture();
	Texture* emissive_texture = material->emissive_texture.texture;
	if (!emissive_texture) emissive_texture = Texture::getWhiteTexture();
	Texture* normal_texture = material->normal_texture.texture;
	if (!normal_texture) read_normal = false;

	//select the	
	if (material->alpha_mode == GTR::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (material->two_sided)
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
		case SHOW_MULTI: shader = Shader::Get("light_multipass"); break;
		case SHOW_DEPTH: shader = Shader::Get("texture"); break;
	}


	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	//upload uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	float t = getTime();
	shader->setUniform("u_time", t);

	shader->setUniform("u_color", material->color);
	shader->setUniform("u_emissive_factor", material->emissive_factor);
	if (texture) shader->setUniform("u_texture", texture, 0);
	if (metallic_rougness_texture) shader->setUniform("u_metallic_roughness_texture", metallic_rougness_texture, 1);
	if (emissive_texture) shader->setUniform("u_emissive_texture", emissive_texture, 2);
	if (normal_texture) shader->setUniform("u_normal_texture", normal_texture, 3);

	shader->setUniform("u_ambient_light", scene->ambient_light);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::eAlphaMode::MASK ? material->alpha_cutoff : 0);

	shader->setUniform("u_camera_nearfar", Vector2(camera->near_plane, camera->far_plane));

	// SINGLEPASS
	if (render_mode == DEFAULT) {
		for (int i = 0; i < scene->l_entities.size(); ++i) 
		{
			LightEntity* lent = scene->l_entities[i];
			if (lent->name == "headlight1") {
				shader->setUniform("u_spot_light_pos", lent->model.bottomVector());
				shader->setUniform("u_spot_direction", lent->target);
				shader->setUniform("u_spot_color", lent->color);
				shader->setUniform("u_spotCosineCutoff", cos(lent->cone_angle));
				shader->setUniform("u_spotExponent", (1 / lent->area_size));
				shader->setUniform("u_spot_maxdist", lent->max_distance);
				shader->setUniform("u_spot_visible", lent->visible);
			}
			if (lent->name == "moon") {
				shader->setUniform("u_directional_color", lent->color);
				shader->setUniform("u_directional_pos", lent->model.bottomVector());
				shader->setUniform("u_directional_factor", lent->intensity);
				shader->setUniform("u_directional_visible", lent->visible);
			}
			if (lent->name == "lamp") {
				shader->setUniform("u_point_light_pos", lent->model.bottomVector());
				shader->setUniform("u_point_color", lent->color);
				shader->setUniform("u_point_factor", lent->intensity);
				shader->setUniform("u_point_maxdist", lent->max_distance);
				shader->setUniform("u_point_visible", lent->visible);
			}
			shader->setUniform("u_read_normal", read_normal);

		}
	}

	// MULTIPASS
	if (render_mode == SHOW_MULTI) {

		glDepthFunc(GL_LEQUAL);
		glBlendFunc(GL_ONE, GL_ONE);

		// if the material is transparent, set only the ambient light and the emissive light once
		if (material->alpha_mode == GTR::eAlphaMode::BLEND)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			shader->setUniform("u_light_type", (int)0);
			mesh->render(GL_TRIANGLES);
		}
		else {
			for (int i = 0; i < scene->l_entities.size(); ++i) {
				LightEntity* lent = scene->l_entities[i];
				if (!lent->visible)
					continue;

				if (i == 0) glDisable(GL_BLEND);	// first time rendering the mesh
				else glEnable(GL_BLEND);				
						
				// set uniforms
				lent->setUniforms(shader);
				shader->setUniform("u_read_normal", read_normal);

				if (lent->light_type != POINT) {
					shader->setUniform("u_shadowmap", lent->fbo.depth_texture, 4); 
					shader->setUniform("u_have_shadows", true);
					shader->setUniform("u_shadow_viewproj", lent->viewproj_mat);
					shader->setUniform("u_shadow_bias", (float)0.001);
				}
				else shader->setUniform("u_have_shadows", false);

				if (i != 0) {
					shader->setUniform("u_ambient_light", (float)0.0);
					shader->setUniform("u_emissive_factor", (float)0.0);
				}
				
				mesh->render(GL_TRIANGLES);				
			}
		}
	}
	else {
		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);
	}

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS);

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