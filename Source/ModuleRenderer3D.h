#pragma once
#include "Module.h"
#include "Globals.h"
#include "glmath.h"
#include "Light.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <list>
#include <map>

class ComponentMeshRenderer;
class ComponentParticleEmmiter;
class ComponentLight;
class Primitive;
class ComponentCamera;
class ComponentCanvas;
class DebugDraw;
class Material;

#define MAX_LIGHTS 8

#define MAX_DIR_LIGHT 2
#define MAX_SPO_LIGHT 8
#define MAX_POI_LIGHT 8

#define SHADOW_HEIGHT 4096
#define SHADOW_WIDTH 4096

class ModuleRenderer3D : public Module
{
public:
	ModuleRenderer3D(Application* app, bool start_enabled = true, bool is_game = false);
	~ModuleRenderer3D();

	bool Init(Data* editor_config = nullptr);
	update_status PreUpdate(float dt);
	update_status PostUpdate(float dt);
	bool CleanUp();

	void OnResize(int width, int height, ComponentCamera* camera);
	void SetWireframeMode();
	void SaveData(Data* data);

	void SetActiveDepthTest(bool active);
	void SetActiveCullTest(bool active);
	void SetActiveFog(bool active);

	bool GetActiveDepthTest() const;
	bool GetActiveCullTest() const;
	bool GetActiveFog() const;

	void ActiveSkybox(bool active);
	bool IsUsingSkybox()const;

	void AddMeshToDraw(ComponentMeshRenderer* mesh);

	void AddCanvasToDraw(ComponentCanvas* canvas);
	void RemoveCanvasToDraw(ComponentCanvas* canvas);
	std::vector<ComponentCanvas*> GetCanvasToDraw() const;


	void BindArrayBuffer(uint id) const;
	void BindElementArrayBuffer(uint id) const;

	void UnbindArraybuffer() const;
	void UnbindElementArrayBuffer() const;

	void AddLight(ComponentLight* light);
	void RemoveLight(ComponentLight* light);

	DebugDraw* GetDebugDraw() const;

	void ResetRender();

	//Shaders
	uint GenVertexArrayObject() const;
	void BindVertexArrayObject(uint id) const;
	void UnbindVertexArrayObject() const;
	void DeleteVertexArrayObject(uint vao);

	uint CreateVertexShader(const char* source);
	uint CreateFragmentShader(const char* source);
	void DeleteShader(uint shader_id);

	uint GetProgramBinary(uint program_id, uint buff_size, char* buff) const;
	int GetProgramSize(uint program_id) const;
	void LoadProgramFromBinary(uint program_id, uint buff_size, const char* buff);

	void EnableVertexAttributeArray(uint id);
	void DisableVertexAttributeArray(uint id);
	void SetVertexAttributePointer(uint id, uint element_size, uint elements_gap, uint infogap);

	void UseShaderProgram(uint id);
	
	void SetUniformBool(uint program, const char* name, bool data);
	void SetUniformFloat(uint program, const char* name, float data);
	void SetUniformVector2(uint program, const char* name, float2 data);
	void SetUniformVector3(uint program, const char* name, float3 data);
	void SetUniformVector4(uint program, const char* name, float4 data);
	void SetUniformMatrix(uint program, const char* name, float* data);
	void SetUniformUInt(uint program, const char* name, uint data);
	void SetUniformInt(uint program, const char* name, int data);

	uint CreateShaderProgram();
	void AttachShaderToProgram(uint program_id, uint shader_id);
	bool LinkProgram(uint program_id);
	void DeleteProgram(uint program_id);

	//particles
	void AddParticleToDraw(ComponentParticleEmmiter* particle);

	//Debug
	void DrawDebugCube(AABB& aabb, ComponentCamera* active_camera);
	void DrawDebugOBB(OBB& aabb, ComponentCamera* active_camera);
	void DrawMesh(std::vector<ComponentMeshRenderer*> mesh, ComponentCamera* active_camera);
	void DrawMeshWithBlending(ComponentMeshRenderer* mesh, ComponentCamera* active_camera);

	void SendLight(uint program_id);
	int GetDirectionalLightCount() const;
	int GetSpotLightCount() const;
	int GetPointLightCount() const;

	void SetDepthMap();
	void DrawFromLightForShadows();
	void SendObjectToDepthShader(uint program, ComponentMeshRenderer* mesh);

private:
	void DrawSceneGameObjects(ComponentCamera* active_camera, bool is_editor_camera);
	void DrawEditorScene();
	void DrawSceneCameras(ComponentCamera* camera);

	void DrawCanvas(ComponentCamera* camera, bool editor_camera = true);
	
	void OrderByMaterials(ComponentMeshRenderer* mesh, ComponentCamera* active_camera);

public:
	Light lights[MAX_LIGHTS];
	float4x4 OrthoProjection( float left, float right, float bottom, float top, float near_plane, float far_plane);
	void DrawGrid(ComponentCamera* camera);

public:
	SDL_GLContext context;
	mat3x3 NormalMatrix;
	mat4x4 ModelMatrix, ViewMatrix, ProjectionMatrix;
	std::list<ComponentCamera*> rendering_cameras;
	ComponentCamera* editor_camera;
	ComponentCamera* game_camera;
	DebugDraw* debug_draw = nullptr;

private:
	bool use_vsync;
	bool is_using_depth_test;
	bool is_using_cull_test;
	bool is_using_fog;

	bool testing_light;
	bool use_skybox;

	uint current_shaderprogram = 0;
	bool using_program = false;

	int lights_count;

	uint current_shader_program;

	std::vector<ComponentMeshRenderer*> dynamic_mesh_to_draw;

	ComponentLight* dir_lights[MAX_DIR_LIGHT];
	ComponentLight* poi_lights[MAX_POI_LIGHT];
	ComponentLight* spo_lights[MAX_SPO_LIGHT];

	int directional_light_count;
	int point_light_count;
	int spot_light_count;

	std::vector<Primitive*> debug_primitive_to_draw;
	std::vector<ComponentCanvas*> canvas_to_draw;
	std::vector<ComponentParticleEmmiter*> particles_to_draw;

	std::vector<std::vector<ComponentMeshRenderer*>> ordering_by_materials;
	std::map<float, ComponentMeshRenderer*> sorted; // For blending

	std::vector<AABB> cubes_to_draw;
	Material* current_material = nullptr;
	bool changed_material = true;
	float self_alpha = -1.0f;


	// SHADOW MAPPING DON'T TOUCH
	uint depth_map;
	uint depth_mapFBO;
	uint default_buffer = 0;
	float near_plane, far_plane;

	glm::mat4 light_space_mat;
};