#pragma once

#include "imgui.h"
#include "../ResourcesWindow.h"
#include "../Shader.h"

class Texture;
class Mesh;
class Prefab;
class GameObject;
class Material;
class Script;
class Font;
class PhysicsMaterial;
class BlastModel;
class SoundBankResource;
class GOAPGoal;
class GOAPAction;

namespace ImGui
{
#define IMGUI_DEFINE_MATH_OPERATORS
	bool InputResourceTexture(const char* label, Texture** texture);
	bool InputResourceMesh(const char* label, Mesh** mesh);
	bool InputResourcePrefab(const char* label, Prefab** prefab);
	bool InputResourceGameObject(const char* label, GameObject** gameobject, ResourcesWindow::GameObjectFilter filter = ResourcesWindow::GameObjectFilter::GoFilterNone);
	bool InputResourceMaterial(const char* label, Material** mat);
	bool InputResourceScript(const char* label, Script** script);
	bool InputResourcePhysMaterial(const char* label, PhysicsMaterial** phys_mat);
	bool InputResourceBlastModel(const char* label, BlastModel** mesh);
	bool InputResourceShader(const char* label, Shader** shader, Shader::ShaderType type);
	bool InputResourceFont(const char* label, Font** font);
	bool InputResourceAudio(const char* label, SoundBankResource** soundbank);
	bool InputResourceGOAPGoal(const char* label, GOAPGoal** goal);
	bool InputResourceGOAPAction(const char* label, GOAPAction** action);
}

