#include "Material.h"
#include "Texture.h"
#include "Data.h"
#include "Application.h"
#include "ModuleMaterialImporter.h"
#include "ModuleFileSystem.h"
#include "ModuleResources.h"
#include "ModuleTextureImporter.h"
#include <ctime>
#include "OpenGL.h"
#include "ShaderProgram.h"
#include "Shader.h"
#include "ModuleRenderer3D.h"

Material::Material()
{
	SetType(Resource::MaterialResource);
	wireframe = false;
	two_sided = false;
	shading_model = 0;
	blend_mode = 0;
	opacity = 1;
	shininess = 0;
	shininess_strength = 1;
	refraction = 1;
	reflectivity = 0;
	bump_scaling = 1;
	diffuse_color = { 0.6f,0.6f,0.0f };
	// Set uvs
	diffuse_UV.x = 1.0f;
	diffuse_UV.y = 1.0f;
	diffuse2_UV.x = 1.0f;
	diffuse2_UV.y = 1.0f;
	specular_UV.x = 1.0f;
	specular_UV.y = 1.0f;
	ambient_UV.x = 1.0f;
	ambient_UV.y = 1.0f;
	emissive_UV.x = 1.0f;
	emissive_UV.y = 1.0f;
	heightmap_UV.x = 1.0f;
	heightmap_UV.y = 1.0f;
	normalmap_UV.x = 1.0f;
	normalmap_UV.y = 1.0f;
	shininess_UV.x = 1.0f;
	shininess_UV.y = 1.0f;
	opacity_UV.x = 1.0f;
	opacity_UV.y = 1.0f;
	displacement_UV.x = 1.0f;
	displacement_UV.y = 1.0f;
	lightmap_UV.x = 1.0f;
	lightmap_UV.y = 1.0f;
	reflection_UV.x = 1.0f;
	reflection_UV.y = 1.0;

	SetDefaultShaders();
}

Material::~Material()
{

}

void Material::Save(Data & data) const
{
	data.AddString("library_path", GetLibraryPath());
	data.AddString("assets_path", GetAssetsPath());
	data.AddString("material_name", GetName());
	data.AddUInt("UUID", GetUID());
	data.AddBool("wireframe", wireframe);
	data.AddBool("two_sided", two_sided);
	data.AddInt("shading", shading_model);
	data.AddInt("blend", blend_mode);
	data.AddFloat("opacity", opacity);
	data.AddFloat("shininess", shininess);
	data.AddFloat("shininess_strength", shininess_strength);
	data.AddFloat("refraction", refraction);
	data.AddFloat("reflectivity", reflectivity);
	data.AddFloat("bump_scaling", bump_scaling);

	if(diffuse_texture != nullptr)
		data.AddString("diffuse_texture", diffuse_texture->GetLibraryPath());
	data.AddFloat("diffuse_UVx", diffuse_UV.x);
	data.AddFloat("diffuse_UVy", diffuse_UV.y);

	math::float4 f_diffuse_color;
	f_diffuse_color.x = diffuse_color.r;
	f_diffuse_color.y = diffuse_color.g;
	f_diffuse_color.z = diffuse_color.b;
	f_diffuse_color.w = diffuse_color.a;
	data.AddVector4("diffuse_color", f_diffuse_color);


	if (diffuse2_texture != nullptr)
		data.AddString("diffuse2_texture", diffuse2_texture->GetLibraryPath());
	data.AddFloat("diffuse2_UVx", diffuse2_UV.x);
	data.AddFloat("diffuse2_UVy", diffuse2_UV.y);

	if (specular_texture != nullptr)
		data.AddString("specular_texture", specular_texture->GetLibraryPath());
	data.AddFloat("specular_UVx", specular_UV.x);
	data.AddFloat("specular_UVy", specular_UV.y);

	if (ambient_texture != nullptr)
		data.AddString("ambient_texture", ambient_texture->GetLibraryPath());
	data.AddFloat("ambient_UVx", ambient_UV.x);
	data.AddFloat("ambient_UVy", ambient_UV.y);

	if (emissive_texture != nullptr) 
		data.AddString("emissive_texture", emissive_texture->GetLibraryPath());
	data.AddFloat("emissive_UVx", emissive_UV.x);
	data.AddFloat("emissive_UVy", emissive_UV.y);

	if (heightmap_texture != nullptr)
		data.AddString("height_texture", heightmap_texture->GetLibraryPath());
	data.AddFloat("heightmap_UVx", heightmap_UV.x);
	data.AddFloat("heightmap_UVy", heightmap_UV.y);

	if (normalmap_texture != nullptr)
		data.AddString("normalmap_texture", normalmap_texture->GetLibraryPath());
	data.AddFloat("normalmap_UVx", normalmap_UV.x);
	data.AddFloat("normalmap_UVy", normalmap_UV.y);

	if (shininess_texture != nullptr)
		data.AddString("shininess_texture", shininess_texture->GetLibraryPath());
	data.AddFloat("shininess_UVx", shininess_UV.x);
	data.AddFloat("shininess_UVy", shininess_UV.y);

	if (opacity_texture != nullptr)
		data.AddString("opacity_texture", opacity_texture->GetLibraryPath());
	data.AddFloat("opacity_UVx", opacity_UV.x);
	data.AddFloat("opacity_UVy", opacity_UV.y);

	if (displacement_texture != nullptr)
		data.AddString("displacement_texture", displacement_texture->GetLibraryPath());
	data.AddFloat("displacement_UVx", displacement_UV.x);
	data.AddFloat("displacement_UVy", displacement_UV.y);

	if (lightmap_texture != nullptr)
		data.AddString("lightmap_texture", lightmap_texture->GetLibraryPath());
	data.AddFloat("lightmap_UVx", lightmap_UV.x);
	data.AddFloat("lightmap_UVy", lightmap_UV.y);

	if (reflection_texture != nullptr)
		data.AddString("reflection_texture", reflection_texture->GetLibraryPath());
	data.AddFloat("reflection_UVx", reflection_UV.x);
	data.AddFloat("reflection_UVy", reflection_UV.y);

	data.AddBool("own_diffuse_uvs", own_diffuse_uvs);
	data.AddBool("own_diffuse2_uvs", own_diffuse2_uvs);
	data.AddBool("own_normal_uvs", own_normal_uvs);


	if (shader_program != nullptr)
	{
		data.AddInt("vertex_shader", shader_program->GetVertexShader()->GetUID());
		data.AddInt("fragment_shader", shader_program->GetFragmentShader()->GetUID());
	}
}

bool Material::Load(Data & data)
{
	bool ret = true;

	wireframe = data.GetBool("wireframe");
	two_sided = data.GetBool("two_sided");
	shading_model = data.GetInt("shading");
	blend_mode = data.GetInt("blend");
	opacity = data.GetFloat("opacity");
	shininess = data.GetFloat("shininess");
	shininess_strength = data.GetFloat("shininess_strength");
	refraction = data.GetFloat("refraction");
	reflectivity = data.GetFloat("reflectivity");
	bump_scaling = data.GetFloat("bump_scaling");


	std::string library_path = data.GetString("diffuse_texture");
	if (library_path != "value not found")
	{
		Texture* diffuse = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetDiffuseTexture(diffuse);
	}
	diffuse_UV.x = data.GetFloat("diffuse_UVx");
	diffuse_UV.y = data.GetFloat("diffuse_UVy");
	if (diffuse_UV.x < 0) diffuse_UV.x = 1.0;
	if (diffuse_UV.y < 0) diffuse_UV.y = 1.0;

	math::float4 f_diffuse_color = data.GetVector4("diffuse_color");
	diffuse_color.r = f_diffuse_color.x;
	diffuse_color.g = f_diffuse_color.y;
	diffuse_color.b = f_diffuse_color.z;
	diffuse_color.a = f_diffuse_color.w;
	if (diffuse_color.a == -1.0f) diffuse_color.a = 0.0f;

	library_path = data.GetString("diffuse2_texture");
	if (library_path != "value not found")
	{
		Texture* diffuse2 = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetDiffuse2Texture(diffuse2);
	}
	diffuse2_UV.x = data.GetFloat("diffuse2_UVx");
	diffuse2_UV.y = data.GetFloat("diffuse2_UVy");
	if (diffuse2_UV.x < 0) diffuse2_UV.x = 1.0;
	if (diffuse2_UV.y < 0) diffuse2_UV.y = 1.0;

	library_path = data.GetString("specular_texture");
	if (library_path != "value not found")
	{
		Texture* specular = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetSpecularTexture(specular);
	}
	specular_UV.x = data.GetFloat("specular_UVx");
	specular_UV.y = data.GetFloat("specular_UVy");

	library_path = data.GetString("ambient_texture");
	if (library_path != "value not found")
	{
		Texture* ambient = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetAmbientTexture(ambient);
	}
	ambient_UV.x = data.GetFloat("ambient_UVx");
	ambient_UV.y = data.GetFloat("ambient_UVy");

	library_path = data.GetString("emissive_texture");
	if (library_path != "value not found")
	{
		Texture* emissive = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetEmissiveTexture(emissive);
	}
	emissive_UV.x = data.GetFloat("emissive_UVx");
	emissive_UV.y = data.GetFloat("emissive_UVy");

	library_path = data.GetString("heightmap_texture");
	if (library_path != "value not found")
	{
		Texture* heightmap = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetHeightMapTexture(heightmap);
	}
	heightmap_UV.x = data.GetFloat("heightmap_UVx");
	heightmap_UV.y = data.GetFloat("heightmap_UVy");

	library_path = data.GetString("normalmap_texture");
	if (library_path != "value not found")
	{
		Texture* normalmap = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetNormalMapTexture(normalmap);
	}
	normalmap_UV.x = data.GetFloat("normalmap_UVx");
	normalmap_UV.y = data.GetFloat("normalmap_UVy");
	if (normalmap_UV.x < 0) normalmap_UV.x = 1.0;
	if (normalmap_UV.y < 0) normalmap_UV.y = 1.0;


	library_path = data.GetString("shininess_texture");
	if (library_path != "value not found")
	{
		Texture* shininess = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetShininessTexture(shininess);
	}
	shininess_UV.x = data.GetFloat("shininess_UVx");
	shininess_UV.y = data.GetFloat("shininess_UVy");

	library_path = data.GetString("opacity_texture");
	if (library_path != "value not found")
	{
		Texture* opacity = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetOpacityTexture(opacity);
	}
	opacity_UV.x = data.GetFloat("opacity_UVx");
	opacity_UV.y = data.GetFloat("opacity_UVy");

	library_path = data.GetString("displacement_texture");
	if (library_path != "value not found")
	{
		Texture* displacement = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetDisplacementTexture(displacement);
	}
	displacement_UV.x = data.GetFloat("displacement_UVx");
	displacement_UV.y = data.GetFloat("displacement_UVy");

	library_path = data.GetString("lightmap_texture");
	if (library_path != "value not found")
	{
		Texture* lightmap = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetLightMapTexture(lightmap);
	}
	lightmap_UV.x = data.GetFloat("lightmap_UVx");
	lightmap_UV.y = data.GetFloat("lightmap_UVy");

	library_path = data.GetString("reflection_texture");
	if (library_path != "value not found")
	{
		Texture* reflection = App->texture_importer->LoadTextureFromLibrary(library_path, false);
		SetReflectionTexture(reflection);
	}
	reflection_UV.x = data.GetFloat("reflection_UVx");
	reflection_UV.y = data.GetFloat("reflection_UVy");

	SetUID(data.GetUInt("UUID"));
	SetAssetsPath(data.GetString("assets_path"));
	SetLibraryPath(data.GetString("library_path"));
	SetName(data.GetString("material_name"));

	own_diffuse_uvs = data.GetBool("own_diffuse_uvs");
	own_diffuse2_uvs = data.GetBool("own_diffuse2_uvs");
	own_normal_uvs = data.GetBool("own_normal_uvs");
	//Shader* vert = App->resources->GetShader(data.GetInt("vertex_shader"));
	//Shader* frag = App->resources->GetShader(data.GetInt("fragment_shader"));
	//
	//if(vert != nullptr && frag != nullptr)
	//	SetShaders(vert, frag);

	return ret;
}

void Material::CreateMeta() const
{
	time_t now = time(0);
	char* dt = ctime(&now);

	Data data;
	data.AddInt("Type", GetType());
	data.AddUInt("UUID", GetUID());
	data.AddString("Time_Created", dt);
	data.AddString("Library_path", GetLibraryPath());
	data.SaveAsMeta(GetAssetsPath());

	data.AddFloat("diffuse_UVx", diffuse_UV.x);
	data.AddFloat("diffuse_UVy", diffuse_UV.y);

	data.AddFloat("diffuse2_UVx", diffuse_UV.x);
	data.AddFloat("diffuse2_UVy", diffuse_UV.y);

	data.AddFloat("specular_UVx", specular_UV.x);
	data.AddFloat("specular_UVy", specular_UV.y);

	data.AddFloat("ambient_UVx", ambient_UV.x);
	data.AddFloat("ambient_UVy", ambient_UV.y);

	data.AddFloat("emissive_UVx", emissive_UV.x);
	data.AddFloat("emissive_UVy", emissive_UV.y);

	data.AddFloat("heightmap_UVx", heightmap_UV.x);
	data.AddFloat("heightmap_UVy", heightmap_UV.y);

	data.AddFloat("normalmap_UVx", normalmap_UV.x);
	data.AddFloat("normalmap_UVy", normalmap_UV.y);

	data.AddFloat("shininess_UVx", shininess_UV.x);
	data.AddFloat("shininess_UVy", shininess_UV.y);

	data.AddFloat("opacity_UVx", opacity_UV.x);
	data.AddFloat("opacity_UVy", opacity_UV.y);

	data.AddFloat("displacement_UVx", displacement_UV.x);
	data.AddFloat("displacement_UVy", displacement_UV.y);

	data.AddFloat("lightmap_UVx", lightmap_UV.x);
	data.AddFloat("lightmap_UVy", lightmap_UV.y);

	data.AddFloat("reflection_UVx", reflection_UV.x);
	data.AddFloat("reflection_UVy", reflection_UV.y);

	data.AddBool("own_diffuse_uvs", own_diffuse_uvs);
	data.AddBool("own_diffuse2_uvs", own_diffuse2_uvs);
	data.AddBool("own_normal_uvs", own_normal_uvs);

	data.AddFloat("bump_scaling", bump_scaling);
}


void Material::LoadToMemory()
{
	bool has_tex = false, has_normalmap = false, has_opacity = false, has_tex2 = false;
	if (diffuse_texture != nullptr)
	{
		if(diffuse_texture->GetID() == 0)
		   diffuse_texture->LoadToMemory(); 

		if (diffuse_texture->GetID() != 0)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, diffuse_texture->GetID());
			has_tex = true;
			App->renderer3D->SetUniformBool(GetShaderProgramID(), "own_uvs_diffuse", own_diffuse_uvs);
		}

		if (own_diffuse_uvs == true)
			App->renderer3D->SetUniformVector2(GetShaderProgramID(), "Tex_Diffuse_UV", diffuse_UV);

		if (diffuse2_texture != nullptr)
		{
			if (diffuse2_texture->GetID() == 0)
				diffuse2_texture->LoadToMemory();

			if (diffuse2_texture->GetID() != 0)
			{
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, diffuse2_texture->GetID());
				has_tex2 = true;
			}


			App->renderer3D->SetUniformBool(GetShaderProgramID(), "own_uvs_diffuse2", own_diffuse2_uvs);
			if (own_diffuse2_uvs == true)
				App->renderer3D->SetUniformVector2(GetShaderProgramID(), "Tex_Diffuse2_UV", diffuse2_UV);
		}
	}

	if (normalmap_texture != nullptr)
	{
		if (normalmap_texture->GetID() == 0)
			normalmap_texture->LoadToMemory();

		if (normalmap_texture->GetID() != 0)
		{	
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, normalmap_texture->GetID());
			has_normalmap = true;
		}
	
		App->renderer3D->SetUniformBool(GetShaderProgramID(), "own_uvs_normalmap", own_normal_uvs);
		if (own_normal_uvs == true)
			App->renderer3D->SetUniformVector2(GetShaderProgramID(), "Tex_NormalMap_UV", normalmap_UV);
	}

	if (opacity_texture != nullptr)
	{
		if (opacity_texture->GetID() == 0)
			opacity_texture->LoadToMemory(); 
		if (opacity_texture->GetID() != 0)
		{
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, opacity_texture->GetID());

			has_opacity = true;
		}
	}


	bool has_mat_color = false;
	if (diffuse_color.a != 0.0f)
	{
		App->renderer3D->SetUniformVector4(GetShaderProgramID(), "material_color", float4(diffuse_color.r, diffuse_color.g, diffuse_color.b, diffuse_color.a));
		has_mat_color = true;
	}

	App->renderer3D->SetUniformInt(GetShaderProgramID(), "Tex_Diffuse", 0);
	App->renderer3D->SetUniformInt(GetShaderProgramID(), "Tex_NormalMap", 1);
	App->renderer3D->SetUniformInt(GetShaderProgramID(), "Tex_Opacity", 2);
	App->renderer3D->SetUniformInt(GetShaderProgramID(), "Tex_Diffuse2", 3);

	App->renderer3D->SetUniformBool(GetShaderProgramID(), "has_texture", has_tex);
	App->renderer3D->SetUniformBool(GetShaderProgramID(), "has_texture2", has_tex2);
	App->renderer3D->SetUniformBool(GetShaderProgramID(), "has_normalmap", has_normalmap);
	App->renderer3D->SetUniformBool(GetShaderProgramID(), "has_opacity", has_opacity);
	App->renderer3D->SetUniformBool(GetShaderProgramID(), "has_material_color", has_mat_color);
}


void Material::UnloadFromMemory()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Material::SetDiffuseTexture(Texture * diffuse)
{
	diffuse_texture = diffuse;
}

void Material::SetDiffuse2Texture(Texture * diffuse2)
{
	diffuse2_texture = diffuse2;
}

void Material::SetSpecularTexture(Texture * specular)
{
	specular_texture = specular;
}

void Material::SetAmbientTexture(Texture * ambient)
{
	ambient_texture = ambient;
}

void Material::SetEmissiveTexture(Texture * emissive)
{
	emissive_texture = emissive;
}

void Material::SetHeightMapTexture(Texture * heightmap)
{
	heightmap_texture = heightmap;
}

void Material::SetNormalMapTexture(Texture * normalmap)
{
	normalmap_texture = normalmap;
}

void Material::SetShininessTexture(Texture * shininess)
{
	shininess_texture = shininess;
}

void Material::SetOpacityTexture(Texture * opacity)
{
	opacity_texture = opacity;
}

void Material::SetDisplacementTexture(Texture * displacement)
{
	displacement_texture = displacement;
}

void Material::SetLightMapTexture(Texture * ligthmap)
{
	lightmap_texture = ligthmap;
}

void Material::SetReflectionTexture(Texture * refection)
{
	reflection_texture = refection;
}

Texture * Material::GetDiffuseTexture() const
{
	return diffuse_texture;
}

Texture * Material::GetDiffuse2Texture() const
{
	return diffuse2_texture;
}

Texture * Material::GetSpecularTexture() const
{
	return specular_texture;
}

Texture * Material::GetAmbientTexture() const
{
	return ambient_texture;
}

Texture * Material::GetEmissiveTexture() const
{
	return emissive_texture;
}

Texture * Material::GetHeightMapTexture() const
{
	return heightmap_texture;
}

Texture * Material::GetNormalMapTexture() const
{
	return normalmap_texture;
}

Texture * Material::GetShininessTexture() const
{
	return shininess_texture;
}

Texture * Material::GetOpacityTexture() const
{
	return opacity_texture;
}

Texture * Material::GetDisplacementTexture() const
{
	return displacement_texture;
}

Texture * Material::GetLightMapTexture() const
{
	return lightmap_texture;
}

Texture * Material::GetReflectionTexture() const
{
	return reflection_texture;
}

void Material::SetDiffuseColor(float r, float g, float b)
{
	diffuse_color.r = r;
	diffuse_color.g = g;
	diffuse_color.b = b;
	diffuse_color.a = 1.0f;
}

void Material::SetSpecularColor(float r, float g, float b)
{
	specular_color.r = r;
	specular_color.g = g;
	specular_color.b = b;
	specular_color.a = 1.0f;
}

void Material::SetAmbientColor(float r, float g, float b)
{
	ambient_color.r = r;
	ambient_color.g = g;
	ambient_color.b = b;
	ambient_color.a = 1.0f;
}

void Material::SetEmissiveColor(float r, float g, float b)
{
	emissive_color.r = r;
	emissive_color.g = g;
	emissive_color.b = b;
	emissive_color.a = 1.0f;
}

void Material::SetTransparentColor(float r, float g, float b)
{
	transparent_color.r = r;
	transparent_color.g = g;
	transparent_color.b = b;
	transparent_color.a = 1.0f;
}

void Material::SetReflectiveColor(float r, float g, float b)
{
	reflective_color.r = r;
	reflective_color.g = g;
	reflective_color.b = b;
	transparent_color.a = 1.0f;
}

Color Material::GetDiffuseColor() const
{
	return diffuse_color;
}

Color Material::GetSpecularColor() const
{
	return specular_color;
}

Color Material::GetAmbientColor() const
{
	return ambient_color;
}

Color Material::GetEmissiveColor() const
{
	return emissive_color;
}

Color Material::GetTransparentColor() const
{
	return transparent_color;
}

Color Material::GetReflectiveColor() const
{
	return reflective_color;
}

void Material::SetWireframe(bool wireframe)
{
	this->wireframe = wireframe;
}

bool Material::IsWireFrame() const
{
	return wireframe;
}

void Material::SetTwoSided(bool two_sided)
{
	this->two_sided = two_sided;
}

bool Material::IsTwoSided() const
{
	return two_sided;
}

void Material::SetShadingModel(int shading_model)
{
	this->shading_model = shading_model;
}

int Material::GetShadingModel() const
{
	return shading_model;
}

void Material::SetBlendMode(int blend_mode)
{
	this->blend_mode = blend_mode;
}

int Material::GetBlendMode() const
{
	return blend_mode;
}

void Material::SetOpacity(float opacity)
{
	this->opacity = opacity;
}

float Material::GetOpacity() const
{
	return opacity;
}

void Material::SetShininess(float shininess)
{
	this->shininess = shininess;
}

float Material::GetShininess() const
{
	return shininess;
}

void Material::SetShininessStrength(float shininess_strength)
{
	this->shininess_strength = shininess_strength;
}

float Material::GetShininessStrength() const
{
	return shininess_strength;
}

void Material::SetRefraction(float refraction)
{
	this->refraction = refraction;
}

float Material::GetRefraction() const
{
	return refraction;
}

void Material::SetReflectivity(float reflectivity)
{
	this->reflectivity = reflectivity;
}

float Material::GetReflectivity() const
{
	return reflectivity;
}

void Material::SetBumpScaling(float bump_scaling)
{
	this->bump_scaling = bump_scaling;
}

float Material::GetBumpScaling() const
{
	return bump_scaling;
}

void Material::IncreaseUsedTexturesCount()
{
	if (diffuse_texture != nullptr)
		diffuse_texture->IncreaseUsedCount();
	
	if (specular_texture != nullptr)
		specular_texture->IncreaseUsedCount();
	
	if (ambient_texture != nullptr)
		ambient_texture->IncreaseUsedCount();
	
	if (emissive_texture != nullptr)
		emissive_texture->IncreaseUsedCount();
	
	if (heightmap_texture != nullptr)
		heightmap_texture->IncreaseUsedCount();
	
	if (normalmap_texture != nullptr)
		normalmap_texture->IncreaseUsedCount();
	
	if (shininess_texture != nullptr)
		shininess_texture->IncreaseUsedCount();
	
	if (opacity_texture != nullptr)
		opacity_texture->IncreaseUsedCount();
	
	if (displacement_texture != nullptr)
		displacement_texture->IncreaseUsedCount();
	
	if(lightmap_texture != nullptr)
		lightmap_texture->IncreaseUsedCount();
	
	if(reflection_texture != nullptr)
		reflection_texture->IncreaseUsedCount();
	
}

void Material::DecreaseUsedTexturesCount()
{
	if (diffuse_texture != nullptr)
		diffuse_texture->DecreaseUsedCount();

	if (specular_texture != nullptr)
		specular_texture->DecreaseUsedCount();

	if (ambient_texture != nullptr)
		ambient_texture->DecreaseUsedCount();

	if (emissive_texture != nullptr)
		emissive_texture->DecreaseUsedCount();

	if (heightmap_texture != nullptr)
		heightmap_texture->DecreaseUsedCount();

	if (normalmap_texture != nullptr)
		normalmap_texture->DecreaseUsedCount();

	if (shininess_texture != nullptr)
		shininess_texture->DecreaseUsedCount();

	if (opacity_texture != nullptr)
		opacity_texture->DecreaseUsedCount();

	if (displacement_texture != nullptr)
		displacement_texture->DecreaseUsedCount();

	if (lightmap_texture != nullptr)
		lightmap_texture->DecreaseUsedCount();

	if (reflection_texture != nullptr)
		reflection_texture->DecreaseUsedCount();
}

ShaderProgram * Material::GetShaderProgram() const
{
	return shader_program;
}

void Material::SetVertexShader(Shader * vertex)
{
	if (vertex != shader_program->GetVertexShader())
	{
		shader_program->DecreaseUsedCount();

		ShaderProgram* prog = App->resources->GetShaderProgram(vertex, shader_program->GetFragmentShader());

		if (prog != nullptr)
		{
			shader_program = prog;
		}
		else
		{
			prog = new ShaderProgram();
			prog->SetFragmentShader(shader_program->GetFragmentShader());
			prog->SetVertexShader(vertex);
			prog->LinkShaderProgram();

			App->resources->AddResource(prog);

			shader_program = prog;
		}

		shader_program->IncreaseUsedCount();
	}
}

void Material::SetFragmentShader(Shader * fragment)
{
	if (fragment != shader_program->GetFragmentShader())
	{
		shader_program->DecreaseUsedCount();

		ShaderProgram* prog = App->resources->GetShaderProgram(shader_program->GetVertexShader(), fragment);

		if (prog != nullptr)
		{
			shader_program = prog;
		}
		else
		{
			prog = new ShaderProgram();
			prog->SetFragmentShader(fragment);
			prog->SetVertexShader(shader_program->GetVertexShader());
			prog->LinkShaderProgram();

			App->resources->AddResource(prog);

			shader_program = prog;
		}

		shader_program->IncreaseUsedCount();
	}
}

void Material::SetShaders(Shader * vertex, Shader * fragment)
{
	ShaderProgram* prog = App->resources->GetShaderProgram(vertex, fragment);

	if (prog != nullptr)
	{
		shader_program = prog;
	}
	else
	{
		prog = new ShaderProgram();
		prog->SetFragmentShader(fragment);
		prog->SetVertexShader(vertex);
		prog->LinkShaderProgram();

		App->resources->AddResource(prog);

		shader_program = prog;
	}

	shader_program->IncreaseUsedCount();
	
}

uint Material::GetShaderProgramID() const
{
	return shader_program->GetProgramID();
}

void Material::SetDefaultShaders()
{
	Shader* vert = App->resources->GetShader("default_vertex");
	Shader* frag = App->resources->GetShader("default_fragment");

	if (vert != nullptr && frag != nullptr)
	{
		ShaderProgram* prog = App->resources->GetShaderProgram(vert, frag);

		if (prog != nullptr)
		{
			shader_program = prog;
			prog->IncreaseUsedCount();
		}
		else
			CONSOLE_ERROR("Default Shader Program missing!");
	}
	else
		CONSOLE_ERROR("Default Shaders missing!");
}

