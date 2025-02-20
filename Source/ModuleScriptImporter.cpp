#include "ModuleScriptImporter.h"
#include "Script.h"
#include "Application.h"
#include "ModuleFileSystem.h"
#include <mono/utils/mono-logger.h>
#include <mono/metadata/metadata.h>
#include "CSScript.h"
#include "GameObject.h"
#include "ModuleScene.h"
#include "ModuleResources.h"
#include "PropertiesWindow.h"
#include "Component.h"
#include "ComponentTransform.h"
#include "ComponentRectTransform.h"
#include "ComponentText.h"
#include "ComponentProgressBar.h"
#include "ComponentRadar.h"
#include "ModuleEditor.h"
#include "ComponentFactory.h"
#include "ModuleTime.h"
#include "ModuleInput.h"
#include "ModuleAudio.h"
#include "ComponentAudioSource.h"
#include "ComponentCanvas.h"
#include "ComponentRigidBody.h"
#include "ComponentGOAPAgent.h"
#include "GOAPGoal.h"
#include "ComponentScript.h"
#include "ComponentLight.h"
#include <mono/metadata/attrdefs.h>
#include "TinyXML/tinyxml2.h"
#include "Prefab.h"
#include "ModulePhysics.h"
#include "ComponentCollider.h"
#include "ModuleRenderer3D.h"
#include "DebugDraw.h"
#include "Application.h"
#include "GameWindow.h"

//CSScript* ModuleScriptImporter::current_script = nullptr;
bool ModuleScriptImporter::inside_function = false;
NSScriptImporter* ModuleScriptImporter::ns_importer = nullptr;

ModuleScriptImporter::ModuleScriptImporter(Application* app, bool start_enabled, bool is_game) : Module(app, start_enabled, is_game)
{
	name = "Script_Importer";
	mono_domain = nullptr;
	mono_engine_image = nullptr;
	ns_importer = new NSScriptImporter();
}

ModuleScriptImporter::~ModuleScriptImporter()
{
	RELEASE(ns_importer);
}

void MonoInternalWarning(const char * string, mono_bool is_stdout)
{
	CONSOLE_WARNING("%s", string);
}

void MonoInternalError(const char * string, mono_bool is_stdout)
{
	CONSOLE_ERROR("%s", string);
}

void MonoLogToLog(const char * log_domain, const char * log_level, const char * message, mono_bool fatal, void * user_data)
{
	if (fatal || log_level == "error")
	{
		CONSOLE_ERROR("%s%s", log_domain != nullptr ? ((std::string)log_domain + ": ").c_str() : "", message);
	}
	else if (log_level == "warning")
	{
		CONSOLE_WARNING("%s%s", log_domain != nullptr ? ((std::string)log_domain + ": ").c_str() : "", message);
	}
	else if (log_level == "critical")
	{
		CONSOLE_ERROR("%s%s", log_domain != nullptr ? ((std::string)log_domain + ": ").c_str() : "", message);
	}
	else
	{
		CONSOLE_LOG("%s%s", log_domain != nullptr ? ((std::string)log_domain + ": ").c_str() : "", message);
	}
}

bool ModuleScriptImporter::Init(Data * editor_config)
{
	mono_trace_set_log_handler(MonoLogToLog, this);
	mono_trace_set_print_handler(MonoInternalWarning);
	mono_trace_set_printerr_handler(MonoInternalError);

	mono_path = App->file_system->GetFullPath("mono/");

	mono_set_dirs((mono_path + "lib").c_str(), (mono_path + "etc").c_str());

	mono_domain = mono_jit_init("TheCreator");

	MonoAssembly* engine_assembly = mono_domain_assembly_open(mono_domain, REFERENCE_ASSEMBLIES_FOLDER"TheEngine.dll");
	if (engine_assembly)
	{
		mono_engine_image = mono_assembly_get_image(engine_assembly);
		RegisterAPI();
		DumpEngineDLLInfo(engine_assembly, mono_engine_image, REFERENCE_ASSEMBLIES_FOLDER"TheEngine.xml");
	}
	else
	{
		CONSOLE_ERROR("Can't load 'TheEngine.dll'!");
		return false;
	}

	MonoAssembly* compiler_assembly = mono_domain_assembly_open(mono_domain, REFERENCE_ASSEMBLIES_FOLDER"ScriptCompiler.dll");
	if (compiler_assembly)
	{
		mono_compiler_image = mono_assembly_get_image(compiler_assembly);
	}
	else
	{
		CONSOLE_ERROR("Can't load 'ScriptCompiler.dll'!");
		return false;
	}

	return true;
}

std::string ModuleScriptImporter::ImportScript(std::string path)
{
	std::string ret = "";
	std::string lib;
	std::string result = CompileScript(path, lib);

	if (result != "")
	{
		for (int i = 0; i < result.size(); i++)
			{
			std::string message;
			while (result[i] != '|' && result[i] != '\0')
			{
				message += result[i];
				i++;
			}
			if (message.find("[Warning]") != std::string::npos)
			{
				message.erase(0, 9);
				CONSOLE_WARNING("%s", message.c_str());
			}
			else if (message.find("[Error]") != std::string::npos)
			{
				message.erase(0, 7);
				CONSOLE_ERROR("%s", message.c_str());
			}
			else
			{
				CONSOLE_LOG("%s", message.c_str());
				if (message.find("Compiled!") != std::string::npos)
				{
					return lib;
				}
			}
		}
	}
	else
	{
		return lib;
	}

	return ret;
}

Script * ModuleScriptImporter::LoadScriptFromLibrary(std::string path)
{
	MonoAssembly* assembly = mono_domain_assembly_open(mono_domain, path.c_str());
	if (assembly)
	{
		CSScript* script = DumpAssemblyInfo(assembly);
		if (script != nullptr)
		{
			if (script->LoadScript(path))
			{
				return script;
			}
		}
	}

	return nullptr;
}

MonoDomain * ModuleScriptImporter::GetDomain() const
{
	return mono_domain;
}

MonoImage * ModuleScriptImporter::GetEngineImage() const
{
	return mono_engine_image;
}

std::string ModuleScriptImporter::CompileScript(std::string assets_path, std::string& lib)
{
	std::string script_name = App->file_system->GetFileNameWithoutExtension(assets_path);
	std::string ret;
	if (!App->file_system->DirectoryExist(LIBRARY_SCRIPTS_FOLDER_PATH)) App->file_system->Create_Directory(LIBRARY_SCRIPTS_FOLDER_PATH);
	std::string output_name = LIBRARY_SCRIPTS_FOLDER + script_name + ".dll";
	lib = LIBRARY_SCRIPTS_FOLDER + script_name + ".dll";

	MonoClass* compiler_class = mono_class_from_name(mono_compiler_image, "Compiler", "Compiler");
	if (compiler_class)
	{
		MonoMethod* method = method = mono_class_get_method_from_name(compiler_class, "Compile", 2);

		if (method)
		{
			MonoObject* exception = nullptr;
			void* args[2];
			MonoString* input_path = mono_string_new(mono_domain, assets_path.c_str());
			MonoString* output_path = mono_string_new(mono_domain, output_name.c_str());
			
			args[0] = input_path;
			args[1] = output_path;

			MonoString* handle = (MonoString*)mono_runtime_invoke(method, NULL, args, &exception);

			if (exception)
			{
				mono_print_unhandled_exception(exception);
			}
			else
			{
				ret = mono_string_to_utf8(handle);
			}
		}
	}

	return ret;
}

//void ModuleScriptImporter::SetCurrentScript(CSScript * script)
//{
//	current_script = script;
//}

//CSScript * ModuleScriptImporter::GetCurrentSctipt() const
//{
//	return current_script;
//}

MonoObject* ModuleScriptImporter::AddGameObjectInfoToMono(GameObject * go)
{
	MonoObject* ret = nullptr;

	if (go != nullptr)
	{
		std::vector<GameObject*> to_add;
		to_add.push_back(go);

		ret = ns_importer->GetMonoObjectFromGameObject(go);

		while (!to_add.empty())
		{
			GameObject* curr_go = *to_add.begin();

			if (curr_go == nullptr)
				continue;

			ns_importer->CreateGameObject(curr_go);

			for (Component* comp : curr_go->components_list)
			{
				ns_importer->CreateComponent(comp);
			}

			for (std::vector<GameObject*>::iterator it = curr_go->childs.begin(); it != curr_go->childs.end(); ++it)
			{
				to_add.push_back(*it);
			}

			to_add.erase(to_add.begin());
		}
	}

	return ret;
}

void ModuleScriptImporter::AddGameObjectsInfoToMono(std::list<GameObject*> scene_objects_list)
{
	for (GameObject* go : scene_objects_list)
	{
		AddGameObjectInfoToMono(go);
	}
}

void ModuleScriptImporter::RemoveGameObjectInfoFromMono(GameObject * go)
{
	if (go != nullptr)
	{
		std::vector<GameObject*> to_add;
		to_add.push_back(go);

		while (!to_add.empty())
		{
			GameObject* curr_go = *to_add.begin();

			if (curr_go == nullptr)
				continue;

			ns_importer->RemoveGameObjectFromMonoObjectList(go);

			for (Component* comp : curr_go->components_list)
			{
				ns_importer->RemoveComponentFromMonoObjectList(comp);
			}

			for (std::vector<GameObject*>::iterator it = curr_go->childs.begin(); it != curr_go->childs.end(); ++it)
			{
				to_add.push_back(*it);
			}

			to_add.erase(to_add.begin());
		}
	}
}

void ModuleScriptImporter::RemoveGameObjectInfoFromMono(std::list<GameObject*> scene_objects_list)
{
	for (GameObject* go : scene_objects_list)
	{
		RemoveGameObjectInfoFromMono(go);
	}
}

CSScript* ModuleScriptImporter::DumpAssemblyInfo(MonoAssembly * assembly)
{
	MonoImage* mono_image = mono_assembly_get_image(assembly);
	if (mono_image)
	{
		std::string class_name;
		std::string name_space;
		MonoClass* loaded_script = DumpClassInfo(mono_image, class_name, name_space);

		if (loaded_script != nullptr)
		{
			CSScript* script = new CSScript();
			script->SetDomain(mono_domain);
			script->SetImage(mono_image);
			script->SetName(class_name);
			script->SetClassName(class_name);
			script->SetNameSpace(name_space);
			script->SetClass(loaded_script);
			return script;
		}
	}
}

MonoClass* ModuleScriptImporter::DumpClassInfo(MonoImage * image, std::string& class_name, std::string& name_space)
{
	MonoClass* mono_class = nullptr;

	const MonoTableInfo* table_info = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);

	int rows = mono_table_info_get_rows(table_info);

	for (int i = 1; i < rows; i++) 
	{
		uint32_t cols[MONO_TYPEDEF_SIZE];
		mono_metadata_decode_row(table_info, i, cols, MONO_TYPEDEF_SIZE);
		const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);
		const char* _name_space = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
		mono_class = mono_class_from_name(image, _name_space, name);

		if (mono_class)
		{
			class_name = name;
			name_space = _name_space;
		}
	}

	return mono_class;
}

void ModuleScriptImporter::DumpEngineDLLInfo(MonoAssembly * assembly, MonoImage* image, const char* engine_xml_path)
{
	tinyxml2::XMLDocument doc;
	doc.LoadFile(engine_xml_path);
	tinyxml2::XMLNode* root = doc.FirstChildElement("doc");
	tinyxml2::XMLNode* members_node = root->FirstChildElement("members");
	tinyxml2::XMLNode* member_node = members_node->FirstChildElement("member");

	const MonoTableInfo* class_table_info = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
	int class_table_rows = mono_table_info_get_rows(class_table_info);
	for (int i = 1; i < class_table_rows; i++) {
		DLLClassInfo class_info;
		uint32_t cols[MONO_TYPEDEF_SIZE];
		mono_metadata_decode_row(class_table_info, i, cols, MONO_TYPEDEF_SIZE);
		class_info.name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);
		const char* name_space = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
		MonoClass* m_class = mono_class_from_name(image, name_space, class_info.name.c_str());

		void* properties_iter = nullptr;
		MonoProperty* property;
		while ((property = mono_class_get_properties(m_class, &properties_iter)))
		{
			MonoMethod* method = mono_property_get_get_method(property);
			if (!(mono_method_get_flags(method, NULL) & MONO_METHOD_ATTR_PUBLIC))
			{
				continue;
			}
			DLLPropertyInfo property_info;
			if (mono_method_get_flags(method, NULL) & MONO_METHOD_ATTR_STATIC)
			{
				property_info.is_static = true;
			}
			property_info.name = mono_property_get_name(property);
			MonoMethodSignature* sig = mono_method_get_signature(method, image, 0);
			MonoType* return_type = mono_signature_get_return_type(sig);
			std::string return_name = mono_type_get_name(return_type);
			bool is_system_type = false;
			if (return_name.find("TheEngine") != std::string::npos) return_name.erase(return_name.begin(), return_name.begin() + 10);
			else if (return_name.find("System") != std::string::npos)
			{
				return_name.erase(return_name.begin(), return_name.begin() + 7);
				is_system_type = true;
			}
			if (return_name == "Single") return_name = "Float";
			else if (return_name == "Int32") return_name = "Int";
			if (is_system_type)
			{
				return_name.front() = std::tolower(return_name.front());
			}
			property_info.returning_type = return_name;

			property_info.declaration = return_name + " " + property_info.name + " {" + (mono_property_get_get_method(property) ? "get;" : "") + (mono_property_get_set_method(property) ? "set;" : "") + "}";
			for (tinyxml2::XMLNode* node = member_node; node; node = node->NextSiblingElement("member"))
			{
				tinyxml2::XMLElement* element = node->ToElement();
				if (element)
				{
					const char* attr_name = element->Attribute("name");
					std::string full_name("P:TheEngine." + class_info.name + "." + property_info.name);
					if (attr_name == full_name)
					{
						tinyxml2::XMLNode* summary = element->FirstChildElement("summary");
						tinyxml2::XMLElement* para = summary->FirstChildElement("para");
						if (para)
						{
							if (para->GetText() != NULL)
							{
								property_info.description = para->GetText();
								property_info.declaration += "\n";
							}
						}
						break;
					}
				}
			}
			class_info.properties.push_back(property_info);
		}

		void* fields_iter = nullptr;
		MonoClassField* field;
		while ((field = mono_class_get_fields(m_class, &fields_iter)))
		{
			if (!(mono_field_get_flags(field) & MONO_FIELD_ATTR_PUBLIC))
			{
				continue;
			}
			DLLFieldsInfo field_info;
			if (mono_field_get_flags(field) & MONO_FIELD_ATTR_STATIC)
			{
				field_info.is_static = true;
			}
			field_info.name = mono_field_get_name(field);
			MonoType* return_type = mono_field_get_type(field);
			std::string return_name = mono_type_get_name(return_type);
			bool is_system_type = false;
			if (return_name.find("TheEngine") != std::string::npos) return_name.erase(return_name.begin(), return_name.begin() + 10);
			else if (return_name.find("System") != std::string::npos)
			{
				return_name.erase(return_name.begin(), return_name.begin() + 7);
				is_system_type = true;
			}
			if (return_name == "Single") return_name = "Float";
			else if (return_name == "Int32") return_name = "Int";
			if (is_system_type)
			{
				return_name.front() = std::tolower(return_name.front());
			}
			field_info.returning_type = return_name;
			field_info.declaration = return_name + " " + class_info.name + "." + field_info.name;

			for (tinyxml2::XMLNode* node = member_node; node; node = node->NextSiblingElement("member"))
			{
				tinyxml2::XMLElement* element = node->ToElement();
				if (element)
				{
					const char* attr_name = element->Attribute("name");
					std::string full_name("F:TheEngine." + class_info.name + "." + field_info.name);
					if (attr_name == full_name)
					{
						tinyxml2::XMLNode* summary = element->FirstChildElement("summary");
						tinyxml2::XMLElement* para = summary->FirstChildElement("para");
						if (para)
						{
							if (para->GetText() != NULL)
							{
								field_info.description += para->GetText();
								field_info.declaration += "\n";
							}
						}
						break;
					}
				}
			}
			class_info.fields.push_back(field_info);
		}

		MonoMethod *method;
		void* iter = nullptr;
		while ((method = mono_class_get_methods(m_class, &iter)))
		{
			if (!(mono_method_get_flags(method, NULL) & MONO_METHOD_ATTR_PUBLIC) || mono_method_get_flags(method, NULL) == 6278
				|| mono_method_get_flags(method, NULL) == 2182 || mono_method_get_flags(method, NULL) == 2198)
			{
				continue;
			}
			std::string method_name = mono_method_get_name(method);
			const MonoTableInfo* method_table_info = mono_image_get_table_info(image, MONO_TABLE_METHOD);
			int method_table_rows = mono_table_info_get_rows(method_table_info);
			const MonoTableInfo* param_table_info = mono_image_get_table_info(image, MONO_TABLE_PARAM);
			int param_table_rows = mono_table_info_get_rows(param_table_info);
			uint index = mono_method_get_index(method);
			uint param_index = mono_metadata_decode_row_col(method_table_info, index - 1, MONO_METHOD_PARAMLIST);
			uint lastp = 0;
			if (index < method_table_rows)
				lastp = mono_metadata_decode_row_col(method_table_info, index, MONO_METHOD_PARAMLIST);
			else
				lastp = param_table_rows + 1;
			uint cols[MONO_PARAM_SIZE];

			DLLMethodInfo method_info;
			if (mono_method_get_flags(method, NULL) & MONO_METHOD_ATTR_STATIC) method_info.is_static = true;
			MonoMethodSignature* sig = mono_method_get_signature(method, image, 0);
			MonoType* return_type = mono_signature_get_return_type(sig);
			std::string return_name = mono_type_get_name(return_type);
			bool is_system_type = false;
			if (return_name.find("TheEngine") != std::string::npos) return_name.erase(return_name.begin(), return_name.begin() + 10);
			else if (return_name.find("System") != std::string::npos)
			{
				return_name.erase(return_name.begin(), return_name.begin() + 7);
				is_system_type = true;
			}
			if (return_name == "Single") return_name = "Float";
			else if (return_name == "Int32") return_name = "Int";
			if (is_system_type)
			{
				return_name.front() = std::tolower(return_name.front());
			}
			method_info.returning_type = return_name;
			method_info.method_name = method_name;

			method_info.declaration = return_name + " " + method_name + "(";
			
			void* param_iter = nullptr;
			MonoType* param_type = nullptr;
			int param_index_start = 0;
			int param_count = mono_signature_get_param_count(sig);
			while ((param_type = mono_signature_get_params(sig, &param_iter)))
			{
				DLLMethodParamsInfo param_info;
				std::string param_type_name = mono_type_get_name(param_type);
				param_info.full_type = param_type_name;
				bool is_system_type = false;
				if (param_type_name.find("TheEngine") != std::string::npos) param_type_name.erase(param_type_name.begin(), param_type_name.begin() + 10);
				else if (param_type_name.find("System") != std::string::npos)
				{
					param_type_name.erase(param_type_name.begin(), param_type_name.begin() + 7);
					is_system_type = true;
				}
				if (mono_signature_param_is_out(sig, param_index_start))
				{
					param_info.is_out = true;
					method_info.declaration += "out";
				}
				else if (param_type_name.find('&') != std::string::npos)
				{
					param_info.is_ref = true;
					method_info.declaration += "ref";
				}
				if (param_type_name == "Single") param_type_name = "Float";
				else if (param_type_name == "Int32") param_type_name = "Int";
				if (is_system_type)
				{
					param_type_name.front() = std::tolower(param_type_name.front());
				}
				param_info.type = param_type_name;
				mono_metadata_decode_row(param_table_info, param_index - 1, cols, MONO_PARAM_SIZE);
				const char* param_name = mono_metadata_string_heap(image, cols[MONO_PARAM_NAME]);
				param_info.name = param_name;

				method_info.declaration += param_info.type + " " + param_info.name;
				param_index_start++;
				if (param_index_start != param_count)
				{
					method_info.declaration += ",";
				}
				method_info.params.push_back(param_info);
				param_index++;
			}

			method_info.declaration += ")";

			for (tinyxml2::XMLNode* node = member_node; node; node = node->NextSiblingElement("member"))
			{
				tinyxml2::XMLElement* element = node->ToElement();
				if (element)
				{
					const char* attr_name = element->Attribute("name");
					std::string full_name("M:TheEngine." + class_info.name + "." + method_info.method_name + "(");
					for (int i = 0; i < method_info.params.size(); i++)
					{
						full_name += method_info.params[i].full_type;
						if(i+1 != method_info.params.size())
						{
							full_name += ",";
						}
					}
					full_name += ")";
					if (attr_name == full_name)
					{
						tinyxml2::XMLNode* summary = element->FirstChildElement("summary");
						tinyxml2::XMLElement* para = summary->FirstChildElement("para");
						if (para)
						{
							if (para->GetText() != NULL)
							{
								method_info.description += para->GetText();
								method_info.declaration += "\n";
							}
						}

						tinyxml2::XMLNode* param_node = node->FirstChildElement("param");
						if (param_node)
						{
							for (tinyxml2::XMLNode* param_node_it = param_node; param_node_it; param_node_it = param_node_it->NextSiblingElement("param"))
							{
								tinyxml2::XMLElement* param = param_node_it->ToElement();
								if (param)
								{
									std:string param_name = param->Attribute("name");
									for (DLLMethodParamsInfo param_info : method_info.params)
									{
										if (param_name == param_info.name)
										{
											if (param->GetText() != NULL)
											{
												param_info.description = param->GetText();
												method_info.params_description += "@ " + param_name + ": " + param_info.description + "\n";
											}
											break;
										}
									}
								}
							}
						}
						break;
					}
				}
			}
			class_info.methods.push_back(method_info);
		}
		engine_dll_info.push_back(class_info);
	}
}

void ModuleScriptImporter::RegisterAPI()
{
	//GAMEOBJECT
	mono_add_internal_call("TheEngine.TheGameObject::CreateNewGameObject", (const void*)CreateGameObject);
	mono_add_internal_call("TheEngine.TheGameObject::Destroy", (const void*)DestroyGameObject);
	mono_add_internal_call("TheEngine.TheGameObject::SetName", (const void*)SetGameObjectName);
	mono_add_internal_call("TheEngine.TheGameObject::GetName", (const void*)GetGameObjectName);
	mono_add_internal_call("TheEngine.TheGameObject::SetActive", (const void*)SetGameObjectActive);
	mono_add_internal_call("TheEngine.TheGameObject::IsActive", (const void*)GetGameObjectIsActive);
	mono_add_internal_call("TheEngine.TheGameObject::SetTag", (const void*)SetGameObjectTag);
	mono_add_internal_call("TheEngine.TheGameObject::GetTag", (const void*)GetGameObjectTag);
	mono_add_internal_call("TheEngine.TheGameObject::SetLayer", (const void*)SetGameObjectLayer);
	mono_add_internal_call("TheEngine.TheGameObject::GetLayer", (const void*)GetGameObjectLayer);
	mono_add_internal_call("TheEngine.TheGameObject::SetStatic", (const void*)SetGameObjectStatic);
	mono_add_internal_call("TheEngine.TheGameObject::IsStatic", (const void*)GameObjectIsStatic);
	mono_add_internal_call("TheEngine.TheGameObject::Duplicate", (const void*)DuplicateGameObject);
	mono_add_internal_call("TheEngine.TheGameObject::SetParent", (const void*)SetGameObjectParent);
	mono_add_internal_call("TheEngine.TheGameObject::GetParent", (const void*)GetGameObjectParent);
	mono_add_internal_call("TheEngine.TheGameObject::GetSelf", (const void*)GetSelfGameObject);
	mono_add_internal_call("TheEngine.TheGameObject::GetChild(int)", (const void*)GetGameObjectChild);
	mono_add_internal_call("TheEngine.TheGameObject::GetChild(string)", (const void*)GetGameObjectChildString);
	mono_add_internal_call("TheEngine.TheGameObject::GetChildCount", (const void*)GetGameObjectChildCount);
	mono_add_internal_call("TheEngine.TheGameObject::AddComponent", (const void*)AddComponent);
	mono_add_internal_call("TheEngine.TheGameObject::DestroyComponent", (const void*)DestroyComponent);
	mono_add_internal_call("TheEngine.TheGameObject::GetComponent", (const void*)GetComponent);
	mono_add_internal_call("TheEngine.TheGameObject::GetScript", (const void*)GetScript);
	mono_add_internal_call("TheEngine.TheGameObject::Find", (const void*)FindGameObject);
	mono_add_internal_call("TheEngine.TheGameObject::GetGameObjectsWithTag", (const void*)GetGameObjectsWithTag);
	mono_add_internal_call("TheEngine.TheGameObject::GetGameObjectsMultipleTags", (const void*)GetGameObjectsMultipleTags);
	mono_add_internal_call("TheEngine.TheGameObject::GetObjectsInFrustum", (const void*)GetObjectsInFrustum);
	mono_add_internal_call("TheEngine.TheGameObject::GetAllChilds", (const void*)GetAllChilds);

	//COMPONENT
	mono_add_internal_call("TheEngine.TheComponent::SetComponentActive", (const void*)SetComponentActive);

	//TRANSFORM
	mono_add_internal_call("TheEngine.TheTransform::SetPosition", (const void*)SetPosition);
	mono_add_internal_call("TheEngine.TheTransform::GetPosition", (const void*)GetPosition);
	mono_add_internal_call("TheEngine.TheTransform::SetRotation", (const void*)SetRotation);
	mono_add_internal_call("TheEngine.TheTransform::GetRotation", (const void*)GetRotation);
	mono_add_internal_call("TheEngine.TheTransform::SetScale", (const void*)SetScale);
	mono_add_internal_call("TheEngine.TheTransform::GetScale", (const void*)GetScale);
	mono_add_internal_call("TheEngine.TheTransform::LookAt", (const void*)LookAt);
	mono_add_internal_call("TheEngine.TheTransform::LookAtY", (const void*)LookAtY);
	mono_add_internal_call("TheEngine.TheTransform::GetForward", (const void*)GetForward);
	mono_add_internal_call("TheEngine.TheTransform::GetUp", (const void*)GetUp);
	mono_add_internal_call("TheEngine.TheTransform::GetRight", (const void*)GetRight);
	mono_add_internal_call("TheEngine.TheTransform::RotateAroundAxis", (const void*)RotateAroundAxis);
	mono_add_internal_call("TheEngine.TheTransform::GetQuatRotation", (const void*)GetQuatRotation);
	mono_add_internal_call("TheEngine.TheTransform::SetQuatRotation", (const void*)SetQuatRotation);
	mono_add_internal_call("TheEngine.TheTransform::SetIncrementalRotation", (const void*)SetIncrementalRotation);

	//RECTTRANSFORM
	mono_add_internal_call("TheEngine.TheRectTransform::SetRectPosition", (const void*)SetRectPosition);
	mono_add_internal_call("TheEngine.TheRectTransform::GetRectPosition", (const void*)GetRectPosition);
	mono_add_internal_call("TheEngine.TheRectTransform::SetRectRotation", (const void*)SetRectRotation);
	mono_add_internal_call("TheEngine.TheRectTransform::GetRectRotation", (const void*)GetRectRotation);
	mono_add_internal_call("TheEngine.TheRectTransform::SetRectSize", (const void*)SetRectSize);
	mono_add_internal_call("TheEngine.TheRectTransform::GetRectSize", (const void*)GetRectSize);
	mono_add_internal_call("TheEngine.TheRectTransform::SetRectAnchor", (const void*)SetRectAnchor);
	mono_add_internal_call("TheEngine.TheRectTransform::GetRectAnchor", (const void*)GetRectAnchor);
	mono_add_internal_call("TheEngine.TheRectTransform::GetOnClick", (const void*)GetOnClick);
	mono_add_internal_call("TheEngine.TheRectTransform::GetOnClickDown", (const void*)GetOnClickDown);
	mono_add_internal_call("TheEngine.TheRectTransform::GetOnClickUp", (const void*)GetOnClickUp);
	mono_add_internal_call("TheEngine.TheRectTransform::GetOnMouseOver", (const void*)GetOnMouseOver);
	mono_add_internal_call("TheEngine.TheRectTransform::GetOnMouseEnter", (const void*)GetOnMouseEnter);
	mono_add_internal_call("TheEngine.TheRectTransform::GetOnMouseOut", (const void*)GetOnMouseOut);

	//TEXT
	mono_add_internal_call("TheEngine.TheText::SetText", (const void*)SetText);
	mono_add_internal_call("TheEngine.TheText::GettText", (const void*)GetText);

	//PROGRESSBAR
	mono_add_internal_call("TheEngine.TheProgressBar::SetPercentageProgress", (const void*)SetPercentageProgress);
	mono_add_internal_call("TheEngine.TheProgressBar::GetPercentageProgress", (const void*)GetPercentageProgress);

	//RADAR
	mono_add_internal_call("TheEngine.TheRadar::AddEntity", (const void*)AddEntity);
	mono_add_internal_call("TheEngine.TheRadar::RemoveEntity", (const void*)RemoveEntity);
	mono_add_internal_call("TheEngine.TheRadar::RemoveAllEntities", (const void*)RemoveAllEntities);
	mono_add_internal_call("TheEngine.TheRadar::SetMarkerToEntity", (const void*)SetMarkerToEntity);

	//FACTORY
	mono_add_internal_call("TheEngine.TheFactory::StartFactory", (const void*)StartFactory);
	mono_add_internal_call("TheEngine.TheFactory::ClearFactory", (const void*)ClearFactory);
	mono_add_internal_call("TheEngine.TheFactory::Spawn", (const void*)Spawn);
	mono_add_internal_call("TheEngine.TheFactory::SetSpawnPosition", (const void*)SetSpawnPosition);
	mono_add_internal_call("TheEngine.TheFactory::SetSpawnRotation", (const void*)SetSpawnRotation);
	mono_add_internal_call("TheEngine.TheFactory::SetSpawnScale", (const void*)SetSpawnScale);

	//CANVAS
	mono_add_internal_call("TheEngine.TheCanvas::ControllerIDUp", (const void*)ControllerIDUp);
	mono_add_internal_call("TheEngine.TheCanvas::ControllerIDDown", (const void*)ControllerIDDown);
	mono_add_internal_call("TheEngine.TheCanvas::SetSelectedRectID", (const void*)SetSelectedRectID);


	//VECTOR/QUATERNION
	mono_add_internal_call("TheEngine.TheVector3::ToQuaternion", (const void*)ToQuaternion);
	mono_add_internal_call("TheEngine.TheQuaternion::ToEulerAngles", (const void*)ToEulerAngles);
	mono_add_internal_call("TheEngine.TheVector3::RotateTowards", (const void*)RotateTowards);

	//DATA SAVE/LOAD
	mono_add_internal_call("TheEngine.TheData::AddString", (const void*)AddString);
	mono_add_internal_call("TheEngine.TheData::AddInt", (const void*)AddInt);
	mono_add_internal_call("TheEngine.TheData::AddFloat", (const void*)AddFloat);
	mono_add_internal_call("TheEngine.TheData::GetString", (const void*)GetString);
	mono_add_internal_call("TheEngine.TheData::GetInt", (const void*)GetInt);
	mono_add_internal_call("TheEngine.TheData::GetFloat", (const void*)GetFloat);

	//TIME
	mono_add_internal_call("TheEngine.TheTime::SetTimeScale", (const void*)SetTimeScale);
	mono_add_internal_call("TheEngine.TheTime::GetTimeScale", (const void*)GetTimeScale);
	mono_add_internal_call("TheEngine.TheTime::GetDeltaTime", (const void*)GetDeltaTime);
	mono_add_internal_call("TheEngine.TheTime::GetTimeSinceStart", (const void*)GetTimeSinceStart);

	//INPUT
	mono_add_internal_call("TheEngine.TheInput::IsKeyDown", (const void*)IsKeyDown);
	mono_add_internal_call("TheEngine.TheInput::IsKeyUp", (const void*)IsKeyUp);
	mono_add_internal_call("TheEngine.TheInput::IsKeyRepeat", (const void*)IsKeyRepeat);
	mono_add_internal_call("TheEngine.TheInput::IsMouseButtonDown", (const void*)IsMouseDown);
	mono_add_internal_call("TheEngine.TheInput::IsMouseButtonUp", (const void*)IsMouseUp);
	mono_add_internal_call("TheEngine.TheInput::IsMouseButtonRepeat", (const void*)IsMouseRepeat);
	mono_add_internal_call("TheEngine.TheInput::GetMousePosition", (const void*)GetMousePosition);
	mono_add_internal_call("TheEngine.TheInput::GetMouseXMotion", (const void*)GetMouseXMotion);
	mono_add_internal_call("TheEngine.TheInput::GetMouseYMotion", (const void*)GetMouseYMotion);
	mono_add_internal_call("TheEngine.TheInput::GetControllerButton", (const void*)GetControllerButton);
	mono_add_internal_call("TheEngine.TheInput::GetControllerJoystickMove", (const void*)GetControllerJoystickMove);
	mono_add_internal_call("TheEngine.TheInput::RumbleController", (const void*)RumbleController);

	//CONSOLE
	mono_add_internal_call("TheEngine.TheConsole.TheConsole::Log", (const void*)Log);
	mono_add_internal_call("TheEngine.TheConsole.TheConsole::Warning", (const void*)Warning);
	mono_add_internal_call("TheEngine.TheConsole.TheConsole::Error", (const void*)Error);

	//Audio
	mono_add_internal_call("TheEngine.TheAudio::IsMuted", (const void*)IsMuted);
	mono_add_internal_call("TheEngine.TheAudio::SetMute", (const void*)SetMute);
	mono_add_internal_call("TheEngine.TheAudio::GetVolume", (const void*)GetVolume);
	mono_add_internal_call("TheEngine.TheAudio::GetPitch", (const void*)GetPitch);
	mono_add_internal_call("TheEngine.TheAudio::SetPitch", (const void*)SetPitch);

	mono_add_internal_call("TheEngine.TheAudio::SetRTPCvalue", (const void*)SetRTPCvalue);

	mono_add_internal_call("TheEngine.TheAudioSource::Play", (const void*)Play);
	mono_add_internal_call("TheEngine.TheAudioSource::Stop", (const void*)Stop);
	mono_add_internal_call("TheEngine.TheAudioSource::Send", (const void*)Send);
	mono_add_internal_call("TheEngine.TheAudioSource::SetMyRTPCvalue", (const void*)SetMyRTPCvalue);
	mono_add_internal_call("TheEngine.TheAudioSource::SetState", (const void*)SetState);
	mono_add_internal_call("TheEngine.TheAudioSource::SetVolume", (const void*)SetVolume);

	//EMITER
	mono_add_internal_call("TheEngine.TheParticleEmmiter::Play", (const void*)PlayEmmiter);
	mono_add_internal_call("TheEngine.TheParticleEmmiter::Stop", (const void*)StopEmmiter);
	mono_add_internal_call("TheEngine.TheParticleEmmiter::SetEmitterSpeed", (const void*)SetEmitterSpeed);
	mono_add_internal_call("TheEngine.TheParticleEmmiter::SetParticleSpeed", (const void*)SetParticleSpeed);

	//RIGIDBODY
	mono_add_internal_call("TheEngine.TheRigidBody::SetLinearVelocity", (const void*)SetLinearVelocity);
	mono_add_internal_call("TheEngine.TheRigidBody::SetAngularVelocity", (const void*)SetAngularVelocity);
	mono_add_internal_call("TheEngine.TheRigidBody::SetPosition", (const void*)SetRBPosition);
	mono_add_internal_call("TheEngine.TheRigidBody::SetRotation", (const void*)SetRBRotation);
	mono_add_internal_call("TheEngine.TheRigidBody::AddTorque", (const void*)AddTorque);
	mono_add_internal_call("TheEngine.TheRigidBody::IsKinematic", (const bool*)IsKinematic);
	mono_add_internal_call("TheEngine.TheRigidBody::SetKinematic", (const void*)SetKinematic);
	mono_add_internal_call("TheEngine.TheRigidBody::GetPosition", (const void*)GetRBPosition);

	//GOAP
	mono_add_internal_call("TheEngine.TheGOAPAgent::GetBlackboardVariableB", (const void*)GetBlackboardVariableB);
	mono_add_internal_call("TheEngine.TheGOAPAgent::GetBlackboardVariableF", (const void*)GetBlackboardVariableF);
	mono_add_internal_call("TheEngine.TheGOAPAgent::GetNumGoals", (const void*)GetNumGoals);
	mono_add_internal_call("TheEngine.TheGOAPAgent::SetGoalPriority", (const void*)SetGoalPriority);
	mono_add_internal_call("TheEngine.TheGOAPAgent::GetGoalPriority", (const void*)GetGoalPriority);
	mono_add_internal_call("TheEngine.TheGOAPAgent::CompleteAction", (const void*)CompleteAction);
	mono_add_internal_call("TheEngine.TheGOAPAgent::FailAction", (const void*)FailAction);
	mono_add_internal_call("TheEngine.TheGOAPAgent::GetGoalName", (const void*)GetGoalName);
	mono_add_internal_call("TheEngine.TheGOAPAgent::GetGoalConditionName", (const void*)GetGoalConditionName);
	mono_add_internal_call("TheEngine.TheGOAPAgent::SetBlackboardVariable(string, float)", (const void*)SetBlackboardVariable);
	mono_add_internal_call("TheEngine.TheGOAPAgent::SetBlackboardVariable(string, bool)", (const void*)SetBlackboardVariableB);

	//RANDOM
	mono_add_internal_call("TheEngine.TheRandom::RandomInt", (const void*)RandomInt);
	mono_add_internal_call("TheEngine.TheRandom::RandomFloat", (const void*)RandomFloat);
	mono_add_internal_call("TheEngine.TheRandom::RandomRange", (const void*)RandomRange);

	//SCRIPT
	mono_add_internal_call("TheEngine.TheScript::SetBoolField", (const void*)SetBoolField);
	mono_add_internal_call("TheEngine.TheScript::GetBoolField", (const void*)GetBoolField);
	mono_add_internal_call("TheEngine.TheScript::SetIntField", (const void*)SetIntField);
	mono_add_internal_call("TheEngine.TheScript::GetIntField", (const void*)GetIntField);
	mono_add_internal_call("TheEngine.TheScript::SetFloatField", (const void*)SetFloatField);
	mono_add_internal_call("TheEngine.TheScript::GetFloatField", (const void*)GetFloatField);
	mono_add_internal_call("TheEngine.TheScript::SetDoubleField", (const void*)SetDoubleField);
	mono_add_internal_call("TheEngine.TheScript::GetDoubleField", (const void*)GetDoubleField);
	mono_add_internal_call("TheEngine.TheScript::SetStringField", (const void*)SetStringField);
	mono_add_internal_call("TheEngine.TheScript::GetStringField", (const void*)GetStringField);
	mono_add_internal_call("TheEngine.TheScript::SetGameObjectField", (const void*)SetGameObjectField);
	mono_add_internal_call("TheEngine.TheScript::GetGameObjectField", (const void*)GetGameObjectField);
	mono_add_internal_call("TheEngine.TheScript::SetVector3Field", (const void*)SetVector3Field);
	mono_add_internal_call("TheEngine.TheScript::GetVector3Field", (const void*)GetVector3Field);
	mono_add_internal_call("TheEngine.TheScript::SetQuaternionField", (const void*)SetQuaternionField);
	mono_add_internal_call("TheEngine.TheScript::GetQuaternionField", (const void*)GetQuaternionField);
	mono_add_internal_call("TheEngine.TheScript::CallFunction", (const void*)CallFunction);
	mono_add_internal_call("TheEngine.TheScript::CallFunctionArgs", (const void*)CallFunctionArgs);

	//APPLICATION
	mono_add_internal_call("TheEngine.TheApplication::LoadScene", (const void*)LoadScene);
	mono_add_internal_call("TheEngine.TheApplication::Quit", (const void*)Quit);

	//RESOURCES
	mono_add_internal_call("TheEngine.TheResources::LoadPrefab", (const void*)LoadPrefab);

	//PHYSICS
	mono_add_internal_call("TheEngine.ThePhysics::Explosion", (const void*)Explosion);
	mono_add_internal_call("TheEngine.ThePhysics::InternalRayCast", (const void*)PhysicsRayCast);
	mono_add_internal_call("TheEngine.ThePhysics::InternalRayCastAll", (const void*)PhysicsRayCastAll);

	//COLLIDER
	mono_add_internal_call("TheEngine.TheCollider::GetGameObject", (const void*)ColliderGetGameObject);
	mono_add_internal_call("TheEngine.TheCollider::GetRigidBody", (const void*)ColliderGetRigidBody);
	mono_add_internal_call("TheEngine.TheCollider::IsTrigger", (const void*)ColliderIsTrigger);
	mono_add_internal_call("TheEngine.TheCollider::SetTrigger", (const void*)ColliderSetTrigger);
	mono_add_internal_call("TheEngine.TheCollider::ClosestPoint", (const void*)ClosestPoint);

	//BOX COLLIDER
	mono_add_internal_call("TheEngine.TheBoxCollider::GetBoxColliderCenter", (const void*)GetBoxColliderCenter);
	mono_add_internal_call("TheEngine.TheBoxCollider::SetBoxColliderCenter", (const void*)SetBoxColliderCenter);
	mono_add_internal_call("TheEngine.TheBoxCollider::GetBoxColliderSize", (const void*)GetBoxColliderSize);
	mono_add_internal_call("TheEngine.TheBoxCollider::SetBoxColliderSize", (const void*)SetBoxColliderSize);

	//CAPSULE COLLIDER
	mono_add_internal_call("TheEngine.TheCapsuleCollider::GetCapsuleColliderCenter", (const void*)GetCapsuleColliderCenter);
	mono_add_internal_call("TheEngine.TheCapsuleCollider::SetCapsuleColliderCenter", (const void*)SetCapsuleColliderCenter);
	mono_add_internal_call("TheEngine.TheCapsuleCollider::GetCapsuleColliderRadius", (const void*)GetCapsuleColliderRadius);
	mono_add_internal_call("TheEngine.TheCapsuleCollider::SetCapsuleColliderRadius", (const void*)SetCapsuleColliderRadius);
	mono_add_internal_call("TheEngine.TheCapsuleCollider::GetCapsuleColliderHeight", (const void*)GetCapsuleColliderHeight);
	mono_add_internal_call("TheEngine.TheCapsuleCollider::SetCapsuleColliderHeight", (const void*)SetCapsuleColliderHeight);
	mono_add_internal_call("TheEngine.TheCapsuleCollider::GetCapsuleColliderDirection", (const void*)GetCapsuleColliderDirection);
	mono_add_internal_call("TheEngine.TheCapsuleCollider::SetCapsuleColliderDirection", (const void*)SetCapsuleColliderDirection);

	//SPHERE COLLIDER
	mono_add_internal_call("TheEngine.TheSphereCollider::GetSphereColliderCenter", (const void*)GetSphereColliderCenter);
	mono_add_internal_call("TheEngine.TheSphereCollider::SetSphereColliderCenter", (const void*)SetSphereColliderCenter);
	mono_add_internal_call("TheEngine.TheSphereCollider::GetSphereColliderRadius", (const void*)GetSphereColliderRadius);
	mono_add_internal_call("TheEngine.TheSphereCollider::SetSphereColliderRadius", (const void*)SetSphereColliderRadius);

	//MESH COLLIDER
	mono_add_internal_call("TheEngine.TheMeshCollider::GetMeshColliderConvex", (const void*)GetMeshColliderConvex);
	mono_add_internal_call("TheEngine.TheMeshCollider::SetMeshColliderConvex", (const void*)SetMeshColliderConvex);

	//DEBUG DRAW
	mono_add_internal_call("TheEngine.TheDebug.TheDebugDraw::Line", (const void*)DebugDrawLine);

	//CAMERA
	mono_add_internal_call("TheEngine.TheCamera::SizeX", (const void*)GetSizeX);
	mono_add_internal_call("TheEngine.TheCamera::SizeY", (const void*)GetSizeY);
	mono_add_internal_call("TheEngine.TheCamera::WorldPosToCameraPos", (const void*)WorldPosToScreenPos);
}

void ModuleScriptImporter::SetGameObjectName(MonoObject * object, MonoString * name)
{
	ns_importer->SetGameObjectName(object, name);
}

MonoString* ModuleScriptImporter::GetGameObjectName(MonoObject * object)
{
	return ns_importer->GetGameObjectName(object);
}

void ModuleScriptImporter::SetGameObjectActive(MonoObject * object, mono_bool active)
{
	ns_importer->SetGameObjectActive(object, active);
}

mono_bool ModuleScriptImporter::GetGameObjectIsActive(MonoObject * object)
{
	return ns_importer->GetGameObjectIsActive(object);
}

void ModuleScriptImporter::CreateGameObject(MonoObject * object)
{
	ns_importer->CreateGameObject(object);
}

void ModuleScriptImporter::DestroyGameObject(MonoObject * object)
{
	ns_importer->DestroyGameObject(object);
}

MonoObject* ModuleScriptImporter::GetSelfGameObject()
{
	return ns_importer->GetSelfGameObject();
}

void ModuleScriptImporter::SetGameObjectTag(MonoObject * object, MonoString * name)
{
	ns_importer->SetGameObjectTag(object, name);
}

MonoString* ModuleScriptImporter::GetGameObjectTag(MonoObject * object)
{
	return ns_importer->GetGameObjectTag(object);
}

void ModuleScriptImporter::SetGameObjectLayer(MonoObject * object, MonoString * layer)
{
	ns_importer->SetGameObjectLayer(object, layer);
}

MonoString * ModuleScriptImporter::GetGameObjectLayer(MonoObject * object)
{
	return ns_importer->GetGameObjectLayer(object);
}

void ModuleScriptImporter::SetGameObjectStatic(MonoObject * object, mono_bool value)
{
	ns_importer->SetGameObjectStatic(object, value);
}

mono_bool ModuleScriptImporter::GameObjectIsStatic(MonoObject * object)
{
	return ns_importer->GameObjectIsStatic(object);
}

MonoObject * ModuleScriptImporter::DuplicateGameObject(MonoObject * object)
{
	return ns_importer->DuplicateGameObject(object);
}

void ModuleScriptImporter::SetGameObjectParent(MonoObject * object, MonoObject * parent)
{
	ns_importer->SetGameObjectParent(object, parent);
}

MonoObject* ModuleScriptImporter::GetGameObjectParent(MonoObject * object)
{
	return ns_importer->GetGameObjectParent(object);
}

MonoObject * ModuleScriptImporter::GetGameObjectChild(MonoObject * object, int index)
{
	return ns_importer->GetGameObjectChild(object, index);
}

MonoObject * ModuleScriptImporter::GetGameObjectChildString(MonoObject * object, MonoString * name)
{
	return ns_importer->GetGameObjectChildString(object, name);
}

int ModuleScriptImporter::GetGameObjectChildCount(MonoObject * object)
{
	return ns_importer->GetGameObjectChildCount(object);
}

MonoObject * ModuleScriptImporter::FindGameObject(MonoString * gameobject_name)
{
	return ns_importer->FindGameObject(gameobject_name);
}

MonoArray * ModuleScriptImporter::GetGameObjectsWithTag(MonoString * tag)
{
	return ns_importer->GetGameObjectsWithTag(tag);
}

MonoArray * ModuleScriptImporter::GetGameObjectsMultipleTags(MonoArray * tags)
{
	return ns_importer->GetGameObjectsMultipleTags(tags);
}

MonoArray * ModuleScriptImporter::GetObjectsInFrustum(MonoObject * pos, MonoObject* front, MonoObject* up, float nearPlaneDist, float farPlaneDist )
{
	return ns_importer->GetObjectsInFrustum(pos, front, up, nearPlaneDist, farPlaneDist);
}

MonoArray * ModuleScriptImporter::GetAllChilds(MonoObject * object)
{
	return ns_importer->GetAllChilds(object);
}

void ModuleScriptImporter::SetComponentActive(MonoObject * object, bool active)
{
	ns_importer->SetComponentActive(object, active);
}

MonoObject* ModuleScriptImporter::AddComponent(MonoObject * object, MonoReflectionType* type)
{
	return ns_importer->AddComponent(object, type);
}

MonoObject* ModuleScriptImporter::GetComponent(MonoObject * object, MonoReflectionType * type, int index)
{
	return ns_importer->GetComponent(object, type, index);
}

MonoObject * ModuleScriptImporter::GetScript(MonoObject * object, MonoString * string)
{
	return ns_importer->GetScript(object, string);
}

void ModuleScriptImporter::DestroyComponent(MonoObject * object, MonoObject * cmp)
{
	ns_importer->DestroyComponent(object, cmp);
}

void ModuleScriptImporter::SetPosition(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetPosition(object, vector3);
}

MonoObject* ModuleScriptImporter::GetPosition(MonoObject * object, mono_bool is_global)
{
	return ns_importer->GetPosition(object, is_global);
}

void ModuleScriptImporter::SetRotation(MonoObject * object, MonoObject * vector)
{
	ns_importer->SetRotation(object, vector);
}

MonoObject * ModuleScriptImporter::GetRotation(MonoObject * object, mono_bool is_global)
{
	return ns_importer->GetRotation(object, is_global);
}

void ModuleScriptImporter::SetScale(MonoObject * object, MonoObject * vector)
{
	ns_importer->SetScale(object, vector);
}

MonoObject * ModuleScriptImporter::GetScale(MonoObject * object, mono_bool is_global)
{
	return ns_importer->GetScale(object, is_global);
}

void ModuleScriptImporter::LookAt(MonoObject * object, MonoObject * vector)
{
	ns_importer->LookAt(object, vector);
}

void ModuleScriptImporter::LookAtY(MonoObject * object, MonoObject * vector)
{
	ns_importer->LookAtY(object, vector);
}


void ModuleScriptImporter::SetRectPosition(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetRectPosition(object, vector3);
}

MonoObject * ModuleScriptImporter::GetRectPosition(MonoObject * object)
{
	return ns_importer->GetRectPosition(object);
}

void ModuleScriptImporter::SetRectRotation(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetRectRotation(object, vector3);
}

MonoObject * ModuleScriptImporter::GetRectRotation(MonoObject * object)
{
	return ns_importer->GetRectRotation(object);
}

void ModuleScriptImporter::SetRectSize(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetRectSize(object, vector3);
}

MonoObject * ModuleScriptImporter::GetRectSize(MonoObject * object)
{
	return ns_importer->GetRectSize(object);
}

void ModuleScriptImporter::SetRectAnchor(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetRectAnchor(object, vector3);
}

MonoObject * ModuleScriptImporter::GetRectAnchor(MonoObject * object)
{
	return ns_importer->GetRectAnchor(object);
}

mono_bool ModuleScriptImporter::GetOnClick(MonoObject * object)
{
	return ns_importer->GetOnClick(object);
}

mono_bool ModuleScriptImporter::GetOnClickDown(MonoObject * object)
{
	return ns_importer->GetOnClickDown(object);
}

mono_bool ModuleScriptImporter::GetOnClickUp(MonoObject * object)
{
	return ns_importer->GetOnClickUp(object);
}

mono_bool ModuleScriptImporter::GetOnMouseEnter(MonoObject * object)
{
	return ns_importer->GetOnMouseEnter(object);
}

mono_bool ModuleScriptImporter::GetOnMouseOver(MonoObject * object)
{
	return ns_importer->GetOnMouseOver(object);
}

mono_bool ModuleScriptImporter::GetOnMouseOut(MonoObject * object)
{
	return ns_importer->GetOnMouseOut(object);
}

void ModuleScriptImporter::ControllerIDUp(MonoObject * object)
{
	return ns_importer->ControllerIDUp(object);
}

void ModuleScriptImporter::ControllerIDDown(MonoObject * object)
{
	return ns_importer->ControllerIDDown(object);
}

void ModuleScriptImporter::SetSelectedRectID(MonoObject * object, int new_id)
{
	ns_importer->SetSelectedRectID(object, new_id); 
}

void ModuleScriptImporter::SetColor(MonoObject * object, MonoObject * color)
{
	ns_importer->SetColor(object, color);
}

MonoObject * ModuleScriptImporter::GetColor(MonoObject * object)
{
	return ns_importer->GetColor(object);
}

void ModuleScriptImporter::SetText(MonoObject * object, MonoString* text)
{
	return ns_importer->SetText(object, text);
}

MonoString* ModuleScriptImporter::GetText(MonoObject * object)
{
	return ns_importer->GetText(object);
}

void ModuleScriptImporter::SetPercentageProgress(MonoObject * object, float progress)
{
	ns_importer->SetPercentageProgress(object, progress);
}

float ModuleScriptImporter::GetPercentageProgress(MonoObject * object)
{
	return ns_importer->GetPercentageProgress(object);
}

void ModuleScriptImporter::AddEntity(MonoObject * object, MonoObject * game_object)
{
	ns_importer->AddEntity(object, game_object);
}

void ModuleScriptImporter::RemoveEntity(MonoObject * object, MonoObject * game_object)
{
	ns_importer->RemoveEntity(object, game_object);
}

void ModuleScriptImporter::RemoveAllEntities(MonoObject * object)
{
	ns_importer->RemoveAllEntities(object);
}

void ModuleScriptImporter::SetMarkerToEntity(MonoObject * object, MonoObject * game_object, MonoString * marker_name)
{
	ns_importer->SetMarkerToEntity(object, game_object, marker_name);
}

void ModuleScriptImporter::AddString(MonoString * name, MonoString * string)
{
	ns_importer->AddString(name, string);
}

void ModuleScriptImporter::AddInt(MonoString * name, int value)
{
	ns_importer->AddInt(name, value);
}

void ModuleScriptImporter::AddFloat(MonoString * name, float value)
{
	ns_importer->AddFloat(name, value);
}

MonoString* ModuleScriptImporter::GetString(MonoString * name)
{
	return ns_importer->GetString(name);
}

int ModuleScriptImporter::GetInt(MonoString * name)
{
	return ns_importer->GetInt(name);
}

float ModuleScriptImporter::GetFloat(MonoString * name)
{
	return ns_importer->GetFloat(name);
}

MonoObject * ModuleScriptImporter::GetForward(MonoObject * object)
{
	return ns_importer->GetForward(object);
}

MonoObject * ModuleScriptImporter::GetRight(MonoObject * object)
{
	return ns_importer->GetRight(object);
}

MonoObject * ModuleScriptImporter::GetUp(MonoObject * object)
{
	return ns_importer->GetUp(object);
}

void ModuleScriptImporter::RotateAroundAxis(MonoObject * object, MonoObject * axis, float angle)
{
	ns_importer->RotateAroundAxis(object, axis, angle);
}

void ModuleScriptImporter::SetIncrementalRotation(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetIncrementalRotation(object, vector3);
}

void ModuleScriptImporter::SetQuatRotation(MonoObject * object, MonoObject * quat)
{
	ns_importer->SetQuatRotation(object, quat);
}

MonoObject * ModuleScriptImporter::GetQuatRotation(MonoObject * object)
{
	return ns_importer->GetQuatRotation(object);
}

void ModuleScriptImporter::StartFactory(MonoObject * object)
{
	ns_importer->StartFactory(object);
}

void ModuleScriptImporter::ClearFactory(MonoObject * object)
{
	ns_importer->ClearFactory(object);
}

MonoObject * ModuleScriptImporter::Spawn(MonoObject * object)
{
	return ns_importer->Spawn(object);
}

void ModuleScriptImporter::SetSpawnPosition(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetSpawnPosition(object, vector3);
}

void ModuleScriptImporter::SetSpawnRotation(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetSpawnRotation(object, vector3);
}

void ModuleScriptImporter::SetSpawnScale(MonoObject * object, MonoObject * vector3)
{
	ns_importer->SetSpawnScale(object, vector3);
}

MonoObject * ModuleScriptImporter::ToQuaternion(MonoObject * object)
{
	return ns_importer->ToQuaternion(object);
}

MonoObject * ModuleScriptImporter::ToEulerAngles(MonoObject * object)
{
	return ns_importer->ToEulerAngles(object);
}

MonoObject * ModuleScriptImporter::RotateTowards(MonoObject * current, MonoObject * target, float angle)
{
	return ns_importer->RotateTowards(current, target, angle);
}

void ModuleScriptImporter::SetTimeScale(float scale)
{
	ns_importer->SetTimeScale(scale);
}

float ModuleScriptImporter::GetTimeScale()
{
	return ns_importer->GetTimeScale();
}

float ModuleScriptImporter::GetDeltaTime()
{
	return ns_importer->GetDeltaTime();
}

float ModuleScriptImporter::GetTimeSinceStart()
{
	return ns_importer->GetTimeSinceStart();
}

mono_bool ModuleScriptImporter::IsKeyDown(MonoString * key_name)
{
	return ns_importer->IsKeyDown(key_name);
}

mono_bool ModuleScriptImporter::IsKeyUp(MonoString * key_name)
{
	return ns_importer->IsKeyUp(key_name);
}

mono_bool ModuleScriptImporter::IsKeyRepeat(MonoString * key_name)
{
	return ns_importer->IsKeyRepeat(key_name);
}

mono_bool ModuleScriptImporter::IsMouseDown(int mouse_button)
{
	return ns_importer->IsMouseDown(mouse_button);
}

mono_bool ModuleScriptImporter::IsMouseUp(int mouse_button)
{
	return ns_importer->IsMouseUp(mouse_button);
}

mono_bool ModuleScriptImporter::IsMouseRepeat(int mouse_button)
{
	return ns_importer->IsMouseRepeat(mouse_button);
}

MonoObject * ModuleScriptImporter::GetMousePosition()
{
	return ns_importer->GetMousePosition();
}

int ModuleScriptImporter::GetMouseXMotion()
{
	return ns_importer->GetMouseXMotion();
}

int ModuleScriptImporter::GetMouseYMotion()
{
	return ns_importer->GetMouseYMotion();
}

int ModuleScriptImporter::GetControllerJoystickMove(int pad, MonoString * axis)
{
	return ns_importer->GetControllerJoystickMove(pad, axis);
}

int ModuleScriptImporter::GetControllerButton(int pad, MonoString * button)
{
	return ns_importer->GetControllerButton(pad, button);
}

void ModuleScriptImporter::RumbleController(int pad, float strength, int ms)
{
	ns_importer->RumbleController(pad, strength, ms);
}

void ModuleScriptImporter::Log(MonoObject * object)
{
	MonoObject* exception = nullptr;
	if (object != nullptr)
	{
		MonoString* str = mono_object_to_string(object, &exception);
		if (exception)
		{
			mono_print_unhandled_exception(exception);
		}
		else
		{
			const char* message = mono_string_to_utf8(str);
			CONSOLE_LOG("%s", message);
		}
	}
	else
	{
		CONSOLE_ERROR("Trying to print a null argument!");
	}
}

void ModuleScriptImporter::Warning(MonoObject * object)
{
	MonoObject* exception = nullptr;
	if (object != nullptr)
	{
		MonoString* str = mono_object_to_string(object, &exception);
		if (exception)
		{
			mono_print_unhandled_exception(exception);
		}
		else
		{
			const char* message = mono_string_to_utf8(str);
			CONSOLE_WARNING("%s", message);
		}
	}
	else
	{
		CONSOLE_ERROR("Trying to print a null argument!");
	}
}

void ModuleScriptImporter::Error(MonoObject * object)
{
	MonoObject* exception = nullptr;
	if (object != nullptr)
	{
		MonoString* str2 = mono_object_to_string(object, &exception);
		if (exception)
		{
			mono_print_unhandled_exception(exception);
		}
		else
		{
			const char* message = mono_string_to_utf8(str2);
			CONSOLE_ERROR("%s", message);
		}
	}
	else
	{
		CONSOLE_ERROR("Trying to print a null argument!");
	}
}

bool ModuleScriptImporter::IsMuted()
{
	return ns_importer->IsMuted();
}

void ModuleScriptImporter::SetMute(bool set)
{
	ns_importer->SetMute(set);
}

int ModuleScriptImporter::GetVolume() 
{
	return ns_importer->GetVolume();
}

int ModuleScriptImporter::GetPitch()
{
	return ns_importer->GetPitch();
}

void ModuleScriptImporter::SetPitch(int pitch)
{
	ns_importer->SetPitch(pitch);
}

void ModuleScriptImporter::SetRTPCvalue(MonoString* name, float value)
{
	ns_importer->SetRTPCvalue(name, value);
}

bool ModuleScriptImporter::Play(MonoObject * object, MonoString * name)
{
	return ns_importer->Play(object, name);
}

bool ModuleScriptImporter::Stop(MonoObject * object, MonoString * name)
{
	return ns_importer->Stop(object, name);
}

bool ModuleScriptImporter::Send(MonoObject * object, MonoString * name)
{
	return ns_importer->Send(object, name);
}

bool ModuleScriptImporter::SetMyRTPCvalue(MonoObject * object, MonoString* name, float value)
{
	return ns_importer->SetMyRTPCvalue(object, name, value);
}

void ModuleScriptImporter::SetState(MonoObject* object, MonoString* group, MonoString* state)
{
	ns_importer->SetState(object, group, state);
}

void ModuleScriptImporter::SetVolume(MonoObject* object, int value)
{
	ns_importer->SetVolume(object, value);
}

void  ModuleScriptImporter::PlayEmmiter(MonoObject * object)
{
	ns_importer->PlayEmmiter(object); 
}
void  ModuleScriptImporter::StopEmmiter(MonoObject * object)
{
	ns_importer->StopEmmiter(object);
}

void ModuleScriptImporter::SetEmitterSpeed(MonoObject * object, float speed)
{
	ns_importer->SetEmitterSpeed(object, speed);
}

void ModuleScriptImporter::SetParticleSpeed(MonoObject * object, float speed)
{
	ns_importer->SetParticleSpeed(object, speed);
}

void ModuleScriptImporter::SetLinearVelocity(MonoObject * object, float x, float y, float z)
{
	ns_importer->SetLinearVelocity(object, x, y, z);
}

void ModuleScriptImporter::SetAngularVelocity(MonoObject * object, float x, float y, float z)
{
	ns_importer->SetAngularVelocity(object, x, y, z);
}

void ModuleScriptImporter::AddTorque(MonoObject * object, float x, float y, float z, int force_type)
{
	ns_importer->AddTorque(object, x, y, z, force_type);
}

bool ModuleScriptImporter::IsKinematic(MonoObject * object)
{
	return ns_importer->IsKinematic(object);
}

void ModuleScriptImporter::SetKinematic(MonoObject * object, bool kinematic)
{
	ns_importer->SetKinematic(object, kinematic);
}

void ModuleScriptImporter::SetRBPosition(MonoObject * object, float x, float y, float z)
{
	ns_importer->SetRBPosition(object, x, y, z);
}

void ModuleScriptImporter::SetRBRotation(MonoObject * object, float x, float y, float z)
{
	ns_importer->SetRBRotation(object, x, y, z);
}

MonoObject* ModuleScriptImporter::GetRBPosition(MonoObject * object)
{
	return ns_importer->GetRBPosition(object);
}

mono_bool ModuleScriptImporter::GetBlackboardVariableB(MonoObject * object, MonoString * name)
{
	return ns_importer->GetBlackboardVariableB(object, name);
}

float ModuleScriptImporter::GetBlackboardVariableF(MonoObject * object, MonoString * name)
{
	return ns_importer->GetBlackboardVariableF(object, name);
}

int ModuleScriptImporter::GetNumGoals(MonoObject * object)
{
	return ns_importer->GetNumGoals(object);
}

MonoString * ModuleScriptImporter::GetGoalName(MonoObject * object, int index)
{
	return ns_importer->GetGoalName(object, index);
}

MonoString * ModuleScriptImporter::GetGoalConditionName(MonoObject * object, int index)
{
	return ns_importer->GetGoalConditionName(object, index);
}

void ModuleScriptImporter::SetGoalPriority(MonoObject * object, int index, int priority)
{
	ns_importer->SetGoalPriority(object, index, priority);
}

int ModuleScriptImporter::GetGoalPriority(MonoObject * object, int index)
{
	return ns_importer->GetGoalPriority(object, index);
}

void ModuleScriptImporter::CompleteAction(MonoObject * object)
{
	ns_importer->CompleteAction(object);
}

void ModuleScriptImporter::FailAction(MonoObject * object)
{
	ns_importer->FailAction(object);
}

void ModuleScriptImporter::SetBlackboardVariable(MonoObject * object, MonoString * name, float value)
{
	ns_importer->SetBlackboardVariable(object, name, value);
}

void ModuleScriptImporter::SetBlackboardVariableB(MonoObject * object, MonoString * name, bool value)
{
	ns_importer->SetBlackboardVariable(object, name, value);
}

int ModuleScriptImporter::RandomInt(MonoObject * object)
{
	return ns_importer->RandomInt(object);
}

float ModuleScriptImporter::RandomFloat(MonoObject * object)
{
	return ns_importer->RandomFloat(object);
}

float ModuleScriptImporter::RandomRange(float min, float max)
{
	return ns_importer->RandomRange(min, max);
}

void ModuleScriptImporter::LoadScene(MonoString * scene_name)
{
	ns_importer->LoadScene(scene_name);
}

void ModuleScriptImporter::Quit()
{
	ns_importer->Quit();
}

void ModuleScriptImporter::SetBoolField(MonoObject * object, MonoString * field_name, bool value)
{
	ns_importer->SetBoolField(object, field_name, value);
}

bool ModuleScriptImporter::GetBoolField(MonoObject * object, MonoString * field_name)
{
	return ns_importer->GetBoolField(object, field_name);
}

void ModuleScriptImporter::SetIntField(MonoObject * object, MonoString * field_name, int value)
{
	ns_importer->SetIntField(object, field_name, value);
}

int ModuleScriptImporter::GetIntField(MonoObject * object, MonoString * field_name)
{
	return ns_importer->GetIntField(object, field_name);
}

void ModuleScriptImporter::SetFloatField(MonoObject * object, MonoString * field_name, float value)
{
	ns_importer->SetFloatField(object, field_name, value);
}

float ModuleScriptImporter::GetFloatField(MonoObject * object, MonoString * field_name)
{
	return ns_importer->GetFloatField(object, field_name);
}

void ModuleScriptImporter::SetDoubleField(MonoObject * object, MonoString * field_name, double value)
{
	ns_importer->SetDoubleField(object, field_name, value);
}

double ModuleScriptImporter::GetDoubleField(MonoObject * object, MonoString * field_name)
{
	return ns_importer->GetDoubleField(object, field_name);
}

void ModuleScriptImporter::SetStringField(MonoObject * object, MonoString * field_name, MonoString * value)
{
	ns_importer->SetStringField(object, field_name, value);
}

MonoString * ModuleScriptImporter::GetStringField(MonoObject * object, MonoString * field_name)
{
	return ns_importer->GetStringField(object, field_name);
}

void ModuleScriptImporter::SetGameObjectField(MonoObject * object, MonoString * field_name, MonoObject * value)
{
	ns_importer->SetGameObjectField(object, field_name, value);
}

MonoObject * ModuleScriptImporter::GetGameObjectField(MonoObject * object, MonoString * field_name)
{
	return ns_importer->GetGameObjectField(object, field_name);
}

void ModuleScriptImporter::SetVector3Field(MonoObject * object, MonoString * field_name, MonoObject * value)
{
	ns_importer->SetVector3Field(object, field_name, value);
}

MonoObject * ModuleScriptImporter::GetVector3Field(MonoObject * object, MonoString * field_name)
{
	return ns_importer->GetVector3Field(object, field_name);
}

void ModuleScriptImporter::SetQuaternionField(MonoObject * object, MonoString * field_name, MonoObject * value)
{
	ns_importer->SetQuaternionField(object, field_name, value);
}

MonoObject * ModuleScriptImporter::GetQuaternionField(MonoObject * object, MonoString * field_name)
{
	return ns_importer->GetQuaternionField(object, field_name);
}

void ModuleScriptImporter::CallFunction(MonoObject * object, MonoString * function_name)
{
	ns_importer->CallFunction(object, function_name);
}

MonoObject* ModuleScriptImporter::CallFunctionArgs(MonoObject * object, MonoString * function_name, MonoArray * arr)
{
	return ns_importer->CallFunctionArgs(object, function_name, arr);
}

MonoObject * ModuleScriptImporter::LoadPrefab(MonoString* prefab_name)
{
	return ns_importer->LoadPrefab(prefab_name);
}

void ModuleScriptImporter::Explosion(MonoObject * world_pos, float radius, float explosive_impulse)
{
	ns_importer->Explosion(world_pos, radius, explosive_impulse);
}

MonoObject* ModuleScriptImporter::PhysicsRayCast(MonoObject * origin, MonoObject * direction, float max_distance)
{
	return ns_importer->PhysicsRayCast(origin, direction, max_distance);
}

MonoArray * ModuleScriptImporter::PhysicsRayCastAll(MonoObject * origin, MonoObject * direction, float max_distance)
{
	return ns_importer->PhysicsRayCastAll(origin, direction, max_distance);
}

MonoObject * ModuleScriptImporter::ColliderGetGameObject(MonoObject * object)
{
	return ns_importer->ColliderGetGameObject(object);
}

MonoObject * ModuleScriptImporter::ColliderGetRigidBody(MonoObject * object)
{
	return ns_importer->ColliderGetRigidBody(object);
}

bool ModuleScriptImporter::ColliderIsTrigger(MonoObject * object)
{
	return ns_importer->ColliderIsTrigger(object);
}

void ModuleScriptImporter::ColliderSetTrigger(MonoObject * object, bool trigger)
{
	ns_importer->ColliderSetTrigger(object, trigger);
}

MonoObject * ModuleScriptImporter::ClosestPoint(MonoObject * object, MonoObject * position)
{
	return ns_importer->ClosestPoint(object, position);
}

MonoObject * ModuleScriptImporter::GetBoxColliderCenter(MonoObject * object)
{
	return ns_importer->GetBoxColliderCenter(object);
}

void ModuleScriptImporter::SetBoxColliderCenter(MonoObject * object, MonoObject * center)
{
	ns_importer->SetBoxColliderCenter(object, center);
}

MonoObject * ModuleScriptImporter::GetBoxColliderSize(MonoObject * object)
{
	return ns_importer->GetBoxColliderSize(object);
}

void ModuleScriptImporter::SetBoxColliderSize(MonoObject * object, MonoObject * size)
{
	ns_importer->SetBoxColliderSize(object, size);
}

MonoObject * ModuleScriptImporter::GetCapsuleColliderCenter(MonoObject * object)
{
	return ns_importer->GetCapsuleColliderCenter(object);
}

void ModuleScriptImporter::SetCapsuleColliderCenter(MonoObject * object, MonoObject * center)
{
	ns_importer->SetCapsuleColliderCenter(object, center);
}

float ModuleScriptImporter::GetCapsuleColliderRadius(MonoObject * object)
{
	return ns_importer->GetCapsuleColliderRadius(object);
}

void ModuleScriptImporter::SetCapsuleColliderRadius(MonoObject * object, float radius)
{
	ns_importer->SetCapsuleColliderRadius(object, radius);
}

float ModuleScriptImporter::GetCapsuleColliderHeight(MonoObject * object)
{
	return ns_importer->GetCapsuleColliderHeight(object);
}

void ModuleScriptImporter::SetCapsuleColliderHeight(MonoObject * object, float height)
{
	ns_importer->SetCapsuleColliderHeight(object, height);
}

int ModuleScriptImporter::GetCapsuleColliderDirection(MonoObject * object)
{
	return ns_importer->GetCapsuleColliderDirection(object);
}

void ModuleScriptImporter::SetCapsuleColliderDirection(MonoObject * object, int direction)
{
	ns_importer->SetCapsuleColliderDirection(object, direction);
}

MonoObject * ModuleScriptImporter::GetSphereColliderCenter(MonoObject * object)
{
	return ns_importer->GetSphereColliderCenter(object);
}

void ModuleScriptImporter::SetSphereColliderCenter(MonoObject * object, MonoObject * center)
{
	ns_importer->SetSphereColliderCenter(object, center);
}

float ModuleScriptImporter::GetSphereColliderRadius(MonoObject * object)
{
	return ns_importer->GetSphereColliderRadius(object);
}

void ModuleScriptImporter::SetSphereColliderRadius(MonoObject * object, float radius)
{
	ns_importer->SetSphereColliderRadius(object, radius);
}

bool ModuleScriptImporter::GetMeshColliderConvex(MonoObject * object)
{
	return ns_importer->GetMeshColliderConvex(object);
}

void ModuleScriptImporter::SetMeshColliderConvex(MonoObject * object, bool convex)
{
	ns_importer->SetMeshColliderConvex(object, convex);
}

int ModuleScriptImporter::GetSizeX()
{
	return ns_importer->GetSizeX();
}

int ModuleScriptImporter::GetSizeY()
{
	return ns_importer->GetSizeY();
}

MonoObject * ModuleScriptImporter::WorldPosToScreenPos(MonoObject * from)
{
	return ns_importer->WorldPosToScreenPos(from);
}

void ModuleScriptImporter::DebugDrawLine(MonoObject * from, MonoObject * to, MonoObject * color)
{
	ns_importer->DebugDrawLine(from, to, color);
}


/////////// Non Static Class Defs //////////////

MonoObject* NSScriptImporter::CreateGameObject(GameObject * go)
{
	MonoObject* ret = nullptr;

	if (go != nullptr)
	{
		if (!IsGameObjectAddedToMonoObjectList(go))
		{
			MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheGameObject");
			if (c)
			{
				ret = mono_object_new(App->script_importer->GetDomain(), c);
				if (ret)
				{
					int id = mono_gchandle_new(ret, 1);

					GameObjectMono gom(go, ret, id);
					created_gameobjects.push_back(gom);
				}
			}
		}
	}

	return ret;
}

GameObject * NSScriptImporter::GetGameObjectFromMonoObject(MonoObject * object)
{
	if (object != nullptr)
	{
		for (std::vector<GameObjectMono>::iterator it = created_gameobjects.begin(); it != created_gameobjects.end(); it++)
		{
			if (object == (*it).obj)
			{
				return (*it).go;
			}
		}
	}

	return nullptr;
}

MonoObject * NSScriptImporter::GetMonoObjectFromGameObject(GameObject * go)
{
	if (go != nullptr)
	{
		for (std::vector<GameObjectMono>::iterator it = created_gameobjects.begin(); it != created_gameobjects.end(); it++)
		{
			if (go == (*it).go)
			{
				return (*it).obj;
			}
		}

		return CreateGameObject(go);
	}

	return nullptr;
}

void NSScriptImporter::RemoveGameObjectFromMonoObjectList(GameObject * go)
{
	if (go != nullptr)
	{
		for (std::vector<GameObjectMono>::iterator it = created_gameobjects.begin(); it != created_gameobjects.end(); ++it)
		{
			if ((*it).go == go)
			{
				mono_gchandle_free((*it).mono_id);
				created_gameobjects.erase(it);
				break;
			}
		}
	}
}

void NSScriptImporter::RemoveMonoObjectFromGameObjectList(MonoObject * object)
{
	if (object != nullptr)
	{
		for (std::vector<GameObjectMono>::iterator it = created_gameobjects.begin(); it != created_gameobjects.end(); ++it)
		{
			if ((*it).obj == object)
			{
				mono_gchandle_free((*it).mono_id);
				created_gameobjects.erase(it);
				break;
			}
		}
	}
}

bool NSScriptImporter::IsGameObjectAddedToMonoObjectList(GameObject * go)
{
	bool ret = false;

	for (std::vector<GameObjectMono>::iterator it = created_gameobjects.begin(); it != created_gameobjects.end(); ++it)
	{
		if ((*it).go == go)
		{
			ret = true;
			break;
		}
	}

	return ret;
}

bool NSScriptImporter::IsMonoObjectAddedToGameObjectList(MonoObject * object)
{
	bool ret = false;

	for (std::vector<GameObjectMono>::iterator it = created_gameobjects.begin(); it != created_gameobjects.end(); ++it)
	{
		if ((*it).obj == object)
		{
			ret = true;
			break;
		}
	}

	return ret;
}

MonoObject * NSScriptImporter::CreateComponent(Component * comp)
{
	MonoObject* ret = nullptr;

	if (comp != nullptr)
	{
		if (!IsComponentAddedToMonoObjectList(comp))
		{
			std::string comp_type = CppComponentToCs(comp->GetType());

			MonoClass* c_comp = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", comp_type.c_str());
			if (c_comp)
			{
				ret = mono_object_new(App->script_importer->GetDomain(), c_comp);
				if (ret)
				{
					int mono_id = mono_gchandle_new(ret, 1);
					ComponentMono cm(comp, ret, mono_id);

					created_components.push_back(cm);
					return ret;
				}
			}

			if (comp_type == "TheBoxCollider" || comp_type == "TheSphereCollider" || comp_type == "TheCapsuleCollider" || comp_type == "TheMeshCollider")
			{
				MonoClass* c_comp = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheCollider");
				if (c_comp)
				{
					ret = mono_object_new(App->script_importer->GetDomain(), c_comp);
					if (ret)
					{
						int mono_id = mono_gchandle_new(ret, 1);
						ComponentMono cm(comp, ret, mono_id);

						created_components.push_back(cm);
						return ret;
					}
				}
			}
		}
	}

	return nullptr;
}

Component * NSScriptImporter::GetComponentFromMonoObject(MonoObject * object)
{
	if (object)
	{
		for (std::vector<ComponentMono>::iterator it = created_components.begin(); it != created_components.end(); it++)
		{
			if (object == (*it).obj)
			{
				return (*it).comp;
			}
		}
	}

	return nullptr;
}

MonoObject* NSScriptImporter::GetMonoObjectFromComponent(Component * component)
{
	if (component)
	{
		for (std::vector<ComponentMono>::iterator it = created_components.begin(); it != created_components.end(); it++)
		{
			if (component == (*it).comp)
			{
				return (*it).obj;
			}
		}

		return CreateComponent(component);
	}

	return nullptr;
}

void NSScriptImporter::RemoveComponentFromMonoObjectList(Component * comp)
{
	if (comp != nullptr)
	{
		for (std::vector<ComponentMono>::iterator it = created_components.begin(); it != created_components.end(); it++)
		{
			if (comp == (*it).comp)
			{
				mono_gchandle_free((*it).mono_id);
				created_components.erase(it);
				break;
			}
		}
	}
}

void NSScriptImporter::RemoveMonoObjectFromComponentList(MonoObject * object)
{
	if (object != nullptr)
	{
		for (std::vector<ComponentMono>::iterator it = created_components.begin(); it != created_components.end(); it++)
		{
			if (object == (*it).obj)
			{
				mono_gchandle_free((*it).mono_id);
				created_components.erase(it);
				break;
			}
		}
	}
}

bool NSScriptImporter::IsComponentAddedToMonoObjectList(Component * comp)
{
	bool ret = false;

	for (std::vector<ComponentMono>::iterator it = created_components.begin(); it != created_components.end(); ++it)
	{
		if ((*it).comp == comp)
		{
			ret = true;
			break;
		}
	}

	return ret;
}

bool NSScriptImporter::IsMonoObjectAddedToComponentList(MonoObject * object)
{
	bool ret = false;

	for (std::vector<ComponentMono>::iterator it = created_components.begin(); it != created_components.end(); ++it)
	{
		if ((*it).obj == object)
		{
			ret = true;
			break;
		}
	}

	return ret;
}

void NSScriptImporter::SetGameObjectName(MonoObject * object, MonoString* name)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go && name)
	{
		const char* new_name = mono_string_to_utf8(name);
		go->SetName(new_name);
		App->scene->RenameDuplicatedGameObject(go);
	}
}

MonoString* NSScriptImporter::GetGameObjectName(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go == nullptr) 
		return nullptr;

	return mono_string_new(App->script_importer->GetDomain(), go->GetName().c_str());
}

void NSScriptImporter::SetGameObjectTag(MonoObject * object, MonoString * tag)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr && tag != nullptr)
	{
		const char* new_tag = mono_string_to_utf8(tag);
		go->SetTag(new_tag);
	}
}

MonoString * NSScriptImporter::GetGameObjectTag(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go == nullptr) 
		return nullptr;

	return mono_string_new(App->script_importer->GetDomain(), go->GetTag().c_str());
}

void NSScriptImporter::SetGameObjectLayer(MonoObject * object, MonoString * layer)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (layer != nullptr)
	{
		const char* new_layer = mono_string_to_utf8(layer);
		go->SetLayer(new_layer);
	}
}

MonoString * NSScriptImporter::GetGameObjectLayer(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go == nullptr) 
		return nullptr;

	return mono_string_new(App->script_importer->GetDomain(), go->GetLayer().c_str());
}

void NSScriptImporter::SetGameObjectStatic(MonoObject * object, mono_bool value)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		go->SetStatic(value);
	}
}

mono_bool NSScriptImporter::GameObjectIsStatic(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		return go->IsStatic();
	}

	return false;
}

MonoObject * NSScriptImporter::DuplicateGameObject(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go == nullptr)
	{
		CONSOLE_ERROR("Trying to instantiate a null object");
		return nullptr;
	}

	GameObject* duplicated = App->scene->DuplicateGameObject(go);

	if (duplicated != nullptr)
	{
		App->script_importer->AddGameObjectInfoToMono(duplicated);

		return GetMonoObjectFromGameObject(duplicated);
	}

	return nullptr;
}

void NSScriptImporter::SetGameObjectParent(MonoObject * object, MonoObject * parent)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		go->SetParent(GetGameObjectFromMonoObject(object));
	}
}

MonoObject* NSScriptImporter::GetGameObjectParent(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		GameObject* parent = go->GetParent();

		if (parent != nullptr)
		{
			return GetMonoObjectFromGameObject(parent);
		}
	}

	return nullptr;
}

MonoObject * NSScriptImporter::GetGameObjectChild(MonoObject * object, int index)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		if (index >= 0 && index < go->childs.size())
		{
			std::vector<GameObject*>::iterator it = std::next(go->childs.begin(), index);
			if ((*it) != nullptr)
			{
				return GetMonoObjectFromGameObject(*it);
			}
		}
		else
		{
			CONSOLE_ERROR("GetChild: Index out of bounds");
		}

	}
	
	return nullptr;
}

MonoObject * NSScriptImporter::GetGameObjectChildString(MonoObject * object, MonoString * name)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	const char* s_name = mono_string_to_utf8(name);

	if (go != nullptr)
	{
		for (std::vector<GameObject*>::iterator it = go->childs.begin(); it != go->childs.end(); it++)
		{
			if (*it != nullptr && (*it)->GetName() == s_name)
			{
				return GetMonoObjectFromGameObject(*it);
			}
		}
	}
	
	return nullptr;
}

int NSScriptImporter::GetGameObjectChildCount(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		return go->childs.size();
	}

	return 0;
}

MonoObject * NSScriptImporter::FindGameObject(MonoString * gameobject_name)
{
	if (gameobject_name == nullptr) 
		return nullptr;

	const char* s_name = mono_string_to_utf8(gameobject_name);

	GameObject* go = App->scene->FindGameObjectByName(s_name);

	if (go != nullptr)
	{
		return GetMonoObjectFromGameObject(go);
	}
	else
	{
		CONSOLE_WARNING("Find: Cannot find gameobject %s", s_name);
	}

	return nullptr;
}

MonoArray * NSScriptImporter::GetGameObjectsWithTag(MonoString * tag)
{
	MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheGameObject");

	std::list<GameObject*> objects = App->scene->scene_gameobjects;

	std::vector<GameObject*> found_gameobjects;
	std::string string_tag = mono_string_to_utf8(tag);

	for (GameObject* go : objects)
	{
		if (go->GetTag() == string_tag)
		{
			found_gameobjects.push_back(go);
		}
	}

	MonoArray* scene_objects = mono_array_new(App->script_importer->GetDomain(), c, found_gameobjects.size());

	if (scene_objects != nullptr)
	{
		int index = 0;
		for (GameObject* go : found_gameobjects)
		{
			MonoObject* obj = GetMonoObjectFromGameObject(go);
			mono_array_set(scene_objects, MonoObject*, index, obj);
			++index;

		}
		return scene_objects;
	}
}

MonoArray * NSScriptImporter::GetGameObjectsMultipleTags(MonoArray * tags)
{
	MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheGameObject");

	std::list<GameObject*> objects = App->scene->scene_gameobjects;

	std::vector<GameObject*> found_gameobjects;
	std::vector<std::string> tags_to_find;

	int tags_count = mono_array_length(tags);

	for (int i = 0; i < tags_count; i++)
	{
		std::string tag = mono_string_to_utf8(mono_array_get(tags, MonoString*, i));
		tags_to_find.push_back(tag);
	}

	for (GameObject* go : objects)
	{
		std::string go_tag = go->GetTag();
		for (std::string tag : tags_to_find)
		{
			if (go_tag == tag)
			{
				found_gameobjects.push_back(go);
				break;
			}
		}
	}

	if (c)
	{
		MonoArray* scene_objects = mono_array_new(App->script_importer->GetDomain(), c, found_gameobjects.size());
		if (scene_objects != nullptr)
		{
			int index = 0;
			for (GameObject* go : found_gameobjects)
			{
				MonoObject* obj = GetMonoObjectFromGameObject(go);

				mono_array_set(scene_objects, MonoObject*, index, obj);

				++index;
			}

			return scene_objects;
		}
	}
	return nullptr;
}

MonoArray * NSScriptImporter::GetObjectsInFrustum(MonoObject * pos, MonoObject * front, MonoObject * up, float nearPlaneDist, float farPlaneDist)
{

	MonoClass* c_pos = mono_object_get_class(pos);
	MonoClassField* x_field = mono_class_get_field_from_name(c_pos, "x");
	MonoClassField* y_field = mono_class_get_field_from_name(c_pos, "y");
	MonoClassField* z_field = mono_class_get_field_from_name(c_pos, "z");

	float3 frustum_pos;
	if (x_field) mono_field_get_value(pos, x_field, &frustum_pos.x);
	if (y_field) mono_field_get_value(pos, y_field, &frustum_pos.y);
	if (z_field) mono_field_get_value(pos, z_field, &frustum_pos.z);

	MonoClass* c_front = mono_object_get_class(front);
	x_field = mono_class_get_field_from_name(c_front, "x");
	y_field = mono_class_get_field_from_name(c_front, "y");
	z_field = mono_class_get_field_from_name(c_front, "z");

	float3 frustum_front;
	if (x_field) mono_field_get_value(front, x_field, &frustum_front.x);
	if (y_field) mono_field_get_value(front, y_field, &frustum_front.y);
	if (z_field) mono_field_get_value(front, z_field, &frustum_front.z);

	MonoClass* c_up = mono_object_get_class(up);
	x_field = mono_class_get_field_from_name(c_up, "x");
	y_field = mono_class_get_field_from_name(c_up, "y");
	z_field = mono_class_get_field_from_name(c_up, "z");

	float3 frustum_up;
	if (x_field) mono_field_get_value(up, x_field, &frustum_up.x);
	if (y_field) mono_field_get_value(up, y_field, &frustum_up.y);
	if (z_field) mono_field_get_value(up, z_field, &frustum_up.z);

	Frustum frustum;
	frustum.SetPos(frustum_pos);
	frustum.SetFront(frustum_front);
	frustum.SetUp(frustum_up);

	frustum.SetKind(math::FrustumProjectiveSpace::FrustumSpaceGL, FrustumHandedness::FrustumRightHanded);
	frustum.SetViewPlaneDistances(nearPlaneDist, farPlaneDist);

	float FOV = 60;
	float aspect_ratio = 1.3;
	frustum.SetHorizontalFovAndAspectRatio(FOV*DEGTORAD, aspect_ratio);
	frustum.ComputeProjectionMatrix();

	std::list<GameObject*> objects = App->scene->scene_gameobjects;
	std::list<GameObject*> obj_inFrustum;

	for (std::list<GameObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
	{
		ComponentMeshRenderer* mesh = (ComponentMeshRenderer*)(*it)->GetComponent(Component::CompMeshRenderer);
		if (mesh == nullptr) continue;

		int vertex_num = mesh->bounding_box.NumVertices();

		for (int i = 0; i < 6; i++) //planes of frustum
		{
			int points_out = 0;
			for (int j = 0; j < vertex_num; j++) //vertex number of box
			{
				Plane plane = frustum.GetPlane(i);
				if (plane.IsOnPositiveSide(mesh->bounding_box.CornerPoint(j)))
				{
					points_out++;
				}
			}
			//if all the points are outside of a plane, the gameobject is not inside the frustum
			if (points_out < 8) obj_inFrustum.push_back((*it));
		}
	}
	// return obj_inFrustum; I don't know how to do that :S
	MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheGameObject");

	if (c)
	{
		MonoArray* scene_objects = mono_array_new(App->script_importer->GetDomain(), c, obj_inFrustum.size());
		if (scene_objects)
		{
			int index = 0;
			for (GameObject* go : obj_inFrustum)
			{
				MonoObject* obj = GetMonoObjectFromGameObject(go);

				mono_array_set(scene_objects, MonoObject*, index, obj);

				++index;
			}
			return scene_objects;
		}
	}
	return nullptr;
}

MonoArray * NSScriptImporter::GetAllChilds(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheGameObject");
		std::vector<GameObject*> objects;

		go->GetAllChilds(objects);

		if (c)
		{
			MonoArray* scene_objects = mono_array_new(App->script_importer->GetDomain(), c, objects.size());
			if (scene_objects)
			{
				int index = 0;
				for (GameObject* go : objects)
				{
					MonoObject* obj = GetMonoObjectFromGameObject(go);

					mono_array_set(scene_objects, MonoObject*, index, obj);

					++index;
				}
				return scene_objects;
			}
		}
	}

	return nullptr;
}

void NSScriptImporter::SetComponentActive(MonoObject * object, bool active)
{
	if (object != nullptr)
	{
		Component* cmp = GetComponentFromMonoObject(object);

		if (cmp != nullptr)
			cmp->SetActive(active);
	}
}

MonoObject* NSScriptImporter::AddComponent(MonoObject * object, MonoReflectionType * type)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go)
	{
		MonoType* t = mono_reflection_type_get_type(type);
		std::string name = mono_type_get_name(t);

		const char* comp_name = "";
		Component::ComponentType comp_type = Component::CompUnknown;

		if (name == "TheEngine.TheTransform")
		{
			CONSOLE_ERROR("Can't add Transform component to %s. GameObjects cannot have more than 1 transform.", go->GetName().c_str());
		}
		else if (name == "TheEngine.TheFactory")
		{
			comp_type = Component::CompTransform;
			comp_name = "TheTransform";
		}
		else if (name == "TheEngine.TheProgressBar")
		{
			comp_type = Component::CompProgressBar;
			comp_name = "TheProgressBar";
		}
		else if (name == "TheEngine.TheText")
		{
			comp_type = Component::CompText;
			comp_name = "TheText";
		}
		else if (name == "TheEngine.TheRigidBody")
		{
			comp_type = Component::CompRigidBody;
			comp_name = "TheRigidBody";
		}

		if (comp_type != Component::CompUnknown)
		{
			Component* comp = go->AddComponent(comp_type);

			return CreateComponent(comp);
		}
	}

	return nullptr;
}

MonoObject* NSScriptImporter::GetComponent(MonoObject * object, MonoReflectionType * type, int index)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		MonoType* t = mono_reflection_type_get_type(type);
		std::string name = mono_type_get_name(t);

		const char* comp_name = "";

		if (name == "TheEngine.TheTransform")
		{
			comp_name = "TheTransform";
		}
		else if (name == "TheEngine.TheFactory")
		{
			comp_name = "TheFactory";
		}
		else if (name == "TheEngine.TheRectTransform")
		{
			comp_name = "TheRectTransform";
		}
		else if (name == "TheEngine.TheProgressBar")
		{
			comp_name = "TheProgressBar";
		}
		else if (name == "TheEngine.TheAudioSource")
		{
			comp_name = "TheAudioSource";
		}
		else if (name == "TheEngine.TheParticleEmmiter")
		{
			comp_name = "TheParticleEmmiter";
		}
		else if (name == "TheEngine.TheText")
		{
			comp_name = "TheText";
		}
		else if (name == "TheEngine.TheLight")
		{
			comp_name = "TheLight";
		}
		else if (name == "TheEngine.TheRadar")
		{
			comp_name = "TheRadar";
		}
		else if (name == "TheEngine.TheRigidBody")
		{
			comp_name = "TheRigidBody";
		}
		else if (name == "TheEngine.TheCollider")
		{
			comp_name = "TheCollider";
		}
		else if (name == "TheEngine.TheBoxCollider")
		{
			comp_name = "TheBoxCollider";
		}
		else if (name == "TheEngine.TheCapsuleCollider")
		{
			comp_name = "TheCapsuleCollider";
		}
		else if (name == "TheEngine.TheSphereCollider")
		{
			comp_name = "TheSphereCollider";
		}
		else if (name == "TheEngine.TheCanvas")
		{
			comp_name = "TheCanvas";
		}
		else if (name == "TheEngine.TheMeshCollider")
		{
			comp_name = "TheMeshCollider";
		}
		else if (name == "TheEngine.TheGOAPAgent")
		{
			comp_name = "TheGOAPAgent";
		}
		else if (name == "TheEngine.TheScript")
		{
			comp_name = "TheScript";
		}

		Component::ComponentType cpp_type = CsToCppComponent(comp_name);

		int temp_index = index;
		if (cpp_type != Component::CompUnknown)
		{
			int comp_type_count = 0;
			for (Component* comp : go->components_list)
			{
				Component::ComponentType c_type = comp->GetType();
				if (cpp_type == Component::CompCollider)
				{
					if (c_type == Component::CompBoxCollider || c_type == Component::CompSphereCollider || c_type == Component::CompCapsuleCollider || c_type == Component::CompMeshCollider)
					{
						comp_type_count++;
					}
				}
				else if (c_type == cpp_type)
				{
					comp_type_count++;
				}
			}

			if (comp_type_count <= index)
			{
				if (comp_type_count == 0)
				{
					//CONSOLE_ERROR("%s GetComponent: %s at index (%d) does not exist in %s", current_script->GetName().c_str(), comp_name, index, go->GetName().c_str());
				}
				else
				{
					//CONSOLE_ERROR("GetComponent method: %s index is out of bounds", comp_name);
				}
				return nullptr;
			}

			for (Component* comp : go->components_list)
			{
				if (comp->GetType() == cpp_type)
				{
					if (temp_index == 0)
					{
						return GetMonoObjectFromComponent(comp);
					}
					else
					{
						temp_index--;
					}
				}
			}

			temp_index = index;
		}
		CONSOLE_ERROR("%s component type is unknown...", comp_name);
	}

	return nullptr;
}

MonoObject * NSScriptImporter::GetScript(MonoObject * object, MonoString * string)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		Component::ComponentType cpp_type = Component::ComponentType::CompScript;

		std::string script_name = mono_string_to_utf8(string);

		if (script_name == "PlayerMovement")
		{
			int i = 0;
		}

		int comp_type_count = 0;
		for (Component* comp : go->components_list)
		{
			if (cpp_type == comp->GetType())
			{
				ComponentScript* c_script = (ComponentScript*)comp;
				if (c_script->GetScriptName() == script_name)
				{
					return GetMonoObjectFromComponent(comp);
				}
			}
		}
	}

	return nullptr;
}

void NSScriptImporter::DestroyComponent(MonoObject* object, MonoObject* cmp)
{
	Component* comp = GetComponentFromMonoObject(cmp);
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (comp != nullptr && go != nullptr)
	{
		RemoveComponentFromMonoObjectList(comp);

		go->DestroyComponent(comp);
	}
}

//
void NSScriptImporter::SetPosition(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_pos;

		if (x_field) mono_field_get_value(vector3, x_field, &new_pos.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_pos.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_pos.z);

		ComponentTransform* transform = (ComponentTransform*)comp;
		transform->SetPosition(new_pos);
	}
}

MonoObject* NSScriptImporter::GetPosition(MonoObject * object, mono_bool is_global)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				ComponentTransform* transform = (ComponentTransform*)comp;
				float3 new_pos;
				if (is_global)
				{
					new_pos = transform->GetGlobalPosition();
				}
				else
				{
					new_pos = transform->GetLocalPosition();
				}

				if (x_field) mono_field_set_value(new_object, x_field, &new_pos.x);
				if (y_field) mono_field_set_value(new_object, y_field, &new_pos.y);
				if (z_field) mono_field_set_value(new_object, z_field, &new_pos.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetRotation(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_rot;

		if (x_field) mono_field_get_value(vector3, x_field, &new_rot.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_rot.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_rot.z);

		ComponentTransform* transform = (ComponentTransform*)comp;
		transform->SetRotation(new_rot);
	}
}

MonoObject * NSScriptImporter::GetRotation(MonoObject * object, mono_bool is_global)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				ComponentTransform* transform = (ComponentTransform*)comp;
				float3 new_rot;
				if (is_global)
				{
					new_rot = transform->GetGlobalRotation();
				}
				else
				{
					new_rot = transform->GetLocalRotation();
				}

				if (x_field) mono_field_set_value(new_object, x_field, &new_rot.x);
				if (y_field) mono_field_set_value(new_object, y_field, &new_rot.y);
				if (z_field) mono_field_set_value(new_object, z_field, &new_rot.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetScale(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_scale;

		if (x_field) mono_field_get_value(vector3, x_field, &new_scale.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_scale.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_scale.z);

		ComponentTransform* transform = (ComponentTransform*)comp;
		transform->SetScale(new_scale);
	}
}

MonoObject * NSScriptImporter::GetScale(MonoObject * object, mono_bool is_global)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				ComponentTransform* transform = (ComponentTransform*)comp;
				float3 new_scale;
				if (is_global)
				{
					new_scale = transform->GetGlobalScale();
				}
				else
				{
					new_scale = transform->GetLocalScale();
				}

				if (x_field) mono_field_set_value(new_object, x_field, &new_scale.x);
				if (y_field) mono_field_set_value(new_object, y_field, &new_scale.y);
				if (z_field) mono_field_set_value(new_object, z_field, &new_scale.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::LookAt(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 target_pos;

		if (x_field) mono_field_get_value(vector3, x_field, &target_pos.x);
		if (y_field) mono_field_get_value(vector3, y_field, &target_pos.y);
		if (z_field) mono_field_get_value(vector3, z_field, &target_pos.z);

		ComponentTransform* transform = (ComponentTransform*)comp;
		float3 direction = target_pos - transform->GetGlobalPosition();
		transform->LookAt(direction.Normalized(), float3::unitY);
	}
}

void NSScriptImporter::LookAtY(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 target_pos;

		if (x_field) mono_field_get_value(vector3, x_field, &target_pos.x);
		if (y_field) mono_field_get_value(vector3, y_field, &target_pos.y);
		if (z_field) mono_field_get_value(vector3, z_field, &target_pos.z);

		ComponentTransform* transform = (ComponentTransform*)comp;
		float3 direction = target_pos - transform->GetGlobalPosition();
		transform->LookAtY(direction.Normalized(), float3::unitY);
	}
}

void NSScriptImporter::SetRectPosition(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_pos;

		if (x_field) mono_field_get_value(vector3, x_field, &new_pos.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_pos.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_pos.z);

		ComponentRectTransform* rect_transform = (ComponentRectTransform*)comp;
		rect_transform->SetPos(float2(new_pos.x, new_pos.y));
	}
}

MonoObject * NSScriptImporter::GetRectPosition(MonoObject * object)
{
	return nullptr;
}

MonoObject * NSScriptImporter::GetForward(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				ComponentTransform* transform = (ComponentTransform*)comp;
				float3 forward = transform->GetForward();

				if (x_field) mono_field_set_value(new_object, x_field, &forward.x);
				if (y_field) mono_field_set_value(new_object, y_field, &forward.y);
				if (z_field) mono_field_set_value(new_object, z_field, &forward.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetRectRotation(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{ 
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_rot;

		if (x_field) mono_field_get_value(vector3, x_field, &new_rot.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_rot.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_rot.z);

		ComponentRectTransform* rect_transform = (ComponentRectTransform*)comp;
		rect_transform->SetRotation(new_rot);
	}
}

MonoObject * NSScriptImporter::GetRectRotation(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				ComponentRectTransform* rect_transform = (ComponentRectTransform*)comp;
				float3 new_rot;
				new_rot.x = rect_transform->GetLocalRotation().x;
				new_rot.y = rect_transform->GetLocalRotation().y;
				new_rot.z = rect_transform->GetLocalRotation().z;

				if (x_field) mono_field_set_value(new_object, x_field, &new_rot.x);
				if (y_field) mono_field_set_value(new_object, y_field, &new_rot.y);
				if (z_field) mono_field_set_value(new_object, z_field, &new_rot.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetRectSize(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_size;

		if (x_field) mono_field_get_value(vector3, x_field, &new_size.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_size.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_size.z);

		ComponentRectTransform* rect_transform = (ComponentRectTransform*)comp;
		rect_transform->SetSize(float2(new_size.x, new_size.y));
	}
}

MonoObject * NSScriptImporter::GetRectSize(MonoObject * object)
{
	return nullptr;
}

MonoObject * NSScriptImporter::GetRight(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				ComponentTransform* transform = (ComponentTransform*)comp;
				float3 right = transform->GetRight();

				if (x_field) mono_field_set_value(new_object, x_field, &right.x);
				if (y_field) mono_field_set_value(new_object, y_field, &right.y);
				if (z_field) mono_field_set_value(new_object, z_field, &right.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetRectAnchor(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_anchor;

		if (x_field) mono_field_get_value(vector3, x_field, &new_anchor.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_anchor.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_anchor.z);

		ComponentRectTransform* rect_transform = (ComponentRectTransform*)comp;
		rect_transform->SetAnchor(float2(new_anchor.x, new_anchor.y));
	}
}

MonoObject * NSScriptImporter::GetRectAnchor(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheRectTransform");

		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				ComponentTransform* transform = (ComponentTransform*)comp;
				float3 right = transform->GetRight();

				if (x_field) mono_field_set_value(new_object, x_field, &right.x);
				if (y_field) mono_field_set_value(new_object, y_field, &right.y);
				if (z_field) mono_field_set_value(new_object, z_field, &right.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

mono_bool NSScriptImporter::GetOnClick(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRectTransform* rect_trans = (ComponentRectTransform*)comp;

		if (rect_trans != nullptr)
		{
			return rect_trans->GetOnClick();
		}
	}
	return false;
}

mono_bool NSScriptImporter::GetOnClickDown(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRectTransform* rect_trans = (ComponentRectTransform*)comp;

		if (rect_trans != nullptr)
		{
			return rect_trans->GetOnClickDown();
		}
	}
	return false;
}

mono_bool NSScriptImporter::GetOnClickUp(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRectTransform* rect_trans = (ComponentRectTransform*)comp;

		if (rect_trans != nullptr)
		{
			return rect_trans->GetOnClickUp();
		}
	}
	return false;
}

mono_bool NSScriptImporter::GetOnMouseEnter(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRectTransform* rect_trans = (ComponentRectTransform*)comp;

		if (rect_trans != nullptr)
		{
			return rect_trans->GetOnMouseEnter();
		}
	}
	return false;
}

mono_bool NSScriptImporter::GetOnMouseOver(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRectTransform* rect_trans = (ComponentRectTransform*)comp;

		if (rect_trans != nullptr)
		{
			return rect_trans->GetOnMouseOver();
		}
	}
	return false;
}

mono_bool NSScriptImporter::GetOnMouseOut(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRectTransform* rect_trans = (ComponentRectTransform*)comp;

		if (rect_trans != nullptr)
		{
			return rect_trans->GetOnMouseOut();
		}
	}
	return false;
}

void NSScriptImporter::ControllerIDUp(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentCanvas* canvas = (ComponentCanvas*)comp;

		if (canvas != nullptr)
		{
			return canvas->AdvanceCursor(); 
		}
	}
}

void NSScriptImporter::ControllerIDDown(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentCanvas* canvas = (ComponentCanvas*)comp;

		if (canvas != nullptr)
		{
			return canvas->RegressCursor(); 
		}
	}
}

void NSScriptImporter::SetSelectedRectID(MonoObject * object, int new_id)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentCanvas* canvas = (ComponentCanvas*)comp;

		if (canvas != nullptr)
		{
			canvas->SetCurrentID(new_id); 
		}
	}
}

void NSScriptImporter::SetColor(MonoObject * object, MonoObject * color)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(color);
		MonoClassField* r_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* g_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* b_field = mono_class_get_field_from_name(c, "z");

		float3 new_color;

		if (r_field) mono_field_get_value(color, r_field, &new_color.x);
		if (g_field) mono_field_get_value(color, g_field, &new_color.y);
		if (b_field) mono_field_get_value(color, b_field, &new_color.z);

		ComponentLight* light = (ComponentLight*)comp;
		light->SetRGBColor(new_color);

	}
}

MonoObject * NSScriptImporter::GetColor(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* r_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* g_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* b_field = mono_class_get_field_from_name(c, "z");

				ComponentLight* light = (ComponentLight*)light;
				float3 new_color = light->GetRGBColor();

				if (r_field) mono_field_set_value(new_object, r_field, &new_color.x);
				if (g_field) mono_field_set_value(new_object, g_field, &new_color.y);
				if (b_field) mono_field_set_value(new_object, b_field, &new_color.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetText(MonoObject * object, MonoString* t)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		const char* txt = mono_string_to_utf8(t);

		ComponentText* text = (ComponentText*)comp;
		if (text)
			text->SetText(txt);
	}
}

MonoString* NSScriptImporter::GetText(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentText* text = (ComponentText*)comp;
		return mono_string_new(App->script_importer->GetDomain(), text->GetText().c_str());
	}
	return mono_string_new(App->script_importer->GetDomain(), "");
}

MonoObject * NSScriptImporter::GetUp(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				ComponentTransform* transform = (ComponentTransform*)comp;
				float3 up = transform->GetUp();

				if (x_field) mono_field_set_value(new_object, x_field, &up.x);
				if (y_field) mono_field_set_value(new_object, y_field, &up.y);
				if (z_field) mono_field_set_value(new_object, z_field, &up.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::RotateAroundAxis(MonoObject * object, MonoObject * axis, float angle)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(axis);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 rot_axis;

		if (x_field) mono_field_get_value(axis, x_field, &rot_axis.x);
		if (y_field) mono_field_get_value(axis, y_field, &rot_axis.y);
		if (z_field) mono_field_get_value(axis, z_field, &rot_axis.z);

		ComponentTransform* transform = (ComponentTransform*)comp;
		transform->RotateAroundAxis(rot_axis, angle);
	}
}

void NSScriptImporter::SetIncrementalRotation(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_rot;

		if (x_field) mono_field_get_value(vector3, x_field, &new_rot.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_rot.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_rot.z);

		ComponentTransform* transform = (ComponentTransform*)comp;
		transform->SetIncrementalRotation(new_rot);
	}
}

void NSScriptImporter::SetQuatRotation(MonoObject * object, MonoObject * quat)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(quat);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");
		MonoClassField* w_field = mono_class_get_field_from_name(c, "w");

		Quat new_rot;

		if (x_field) mono_field_get_value(quat, x_field, &new_rot.x);
		if (y_field) mono_field_get_value(quat, y_field, &new_rot.y);
		if (z_field) mono_field_get_value(quat, z_field, &new_rot.z);
		if (w_field) mono_field_get_value(quat, w_field, &new_rot.w);

		ComponentTransform* transform = (ComponentTransform*)comp;
		transform->SetQuatRotation(new_rot);
	}
}

MonoObject * NSScriptImporter::GetQuatRotation(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheQuaternion");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");
				MonoClassField* w_field = mono_class_get_field_from_name(c, "w");

				ComponentTransform* transform = (ComponentTransform*)comp;
				Quat rot = transform->GetQuatRotation();

				if (x_field) mono_field_set_value(new_object, x_field, &rot.x);
				if (y_field) mono_field_set_value(new_object, y_field, &rot.y);
				if (z_field) mono_field_set_value(new_object, z_field, &rot.z);
				if (w_field) mono_field_set_value(new_object, w_field, &rot.w);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetPercentageProgress(MonoObject * object, float progress)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentProgressBar* progres_barr = (ComponentProgressBar*)comp;

		progres_barr->SetProgressPercentage(progress);
	}
}

float NSScriptImporter::GetPercentageProgress(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentProgressBar* progres_barr = (ComponentProgressBar*)comp;
		return progres_barr->GetProgressPercentage();
	}
	return 0.0f;
}

void NSScriptImporter::AddEntity(MonoObject * object, MonoObject * game_object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRadar* radar = (ComponentRadar*)comp;

		GameObject* go = GetGameObjectFromMonoObject(game_object);

		if (radar != nullptr)
		{
			if (go != nullptr)
			{
				radar->AddEntity(go);
			}
		}
	}
}

void NSScriptImporter::RemoveEntity(MonoObject * object, MonoObject * game_object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRadar* radar = (ComponentRadar*)comp;

		GameObject* go = GetGameObjectFromMonoObject(game_object);

		if (radar != nullptr)
		{
			if (go != nullptr)
			{
				radar->RemoveEntity(go);
			}
		}
	}
}

void NSScriptImporter::RemoveAllEntities(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRadar* radar = (ComponentRadar*)comp;

		if (radar != nullptr)
		{
			radar->RemoveAllEntities();
		}
	}
}

void NSScriptImporter::SetMarkerToEntity(MonoObject * object, MonoObject * game_object, MonoString * marker_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRadar* radar = (ComponentRadar*)comp;

		GameObject* go = GetGameObjectFromMonoObject(game_object);

		if (radar != nullptr)
		{
			if (go != nullptr)
			{
				const char* txt = mono_string_to_utf8(marker_name);

				radar->AddMarkerToEntity(go, radar->GetMarker(txt));
			}
		}
	}
}

void NSScriptImporter::StartFactory(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentFactory* factory = (ComponentFactory*)comp;

		if (factory) 
			factory->StartFactory();
	}
}

void NSScriptImporter::ClearFactory(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentFactory* factory = (ComponentFactory*)comp;

		if (factory) 
			factory->ClearFactory();
	}
}

MonoObject * NSScriptImporter::Spawn(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentFactory* factory = (ComponentFactory*)comp;
		GameObject* go = nullptr;
		if (factory)
		{
			go = factory->Spawn();
		}

		if (go)
		{
			return GetMonoObjectFromGameObject(go);
		}
	}
	return nullptr;
}

void NSScriptImporter::SetSpawnPosition(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_pos;

		if (x_field) mono_field_get_value(vector3, x_field, &new_pos.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_pos.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_pos.z);

		ComponentFactory* factory = (ComponentFactory*)comp;

		if (factory) 
			factory->SetSpawnPos(new_pos);
	}
}

void NSScriptImporter::SetSpawnRotation(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_rot;

		if (x_field) mono_field_get_value(vector3, x_field, &new_rot.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_rot.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_rot.z);

		ComponentFactory* factory = (ComponentFactory*)comp;

		if (factory) 
			factory->SetSpawnRotation(new_rot);
	}
}

void NSScriptImporter::SetSpawnScale(MonoObject * object, MonoObject * vector3)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		MonoClass* c = mono_object_get_class(vector3);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_scale;

		if (x_field) mono_field_get_value(vector3, x_field, &new_scale.x);
		if (y_field) mono_field_get_value(vector3, y_field, &new_scale.y);
		if (z_field) mono_field_get_value(vector3, z_field, &new_scale.z);

		ComponentFactory* factory = (ComponentFactory*)comp;

		if (factory) 
			factory->SetSpawnScale(new_scale);
	}
}

void NSScriptImporter::AddString(MonoString * name, MonoString * string)
{
	JSON_File* json = App->scene->GetJSONTool()->LoadJSON(LIBRARY_GAME_DATA);
	if (json == nullptr)
		json = App->scene->GetJSONTool()->CreateJSON(LIBRARY_GAME_DATA);

	if (json != nullptr)
	{
		const char* c_name = mono_string_to_utf8(name);
		const char* c_string = mono_string_to_utf8(string);

		json->SetString(c_name, c_string);

		json->Save();
	}
}

void NSScriptImporter::AddInt(MonoString * name, int value)
{
	JSON_File* json = App->scene->GetJSONTool()->LoadJSON(LIBRARY_GAME_DATA);
	if (json == nullptr)
		json = App->scene->GetJSONTool()->CreateJSON(LIBRARY_GAME_DATA);

	if (json != nullptr)
	{
		const char* c_name = mono_string_to_utf8(name);

		json->SetNumber(c_name, value);

		json->Save();
	}
}

void NSScriptImporter::AddFloat(MonoString * name, float value)
{
	JSON_File* json = App->scene->GetJSONTool()->LoadJSON(LIBRARY_GAME_DATA);
	if (json == nullptr)
		json = App->scene->GetJSONTool()->CreateJSON(LIBRARY_GAME_DATA);

	if (json != nullptr)
	{
		const char* c_name = mono_string_to_utf8(name);

		json->SetNumber(c_name, value);

		json->Save();
	}
}

MonoString* NSScriptImporter::GetString(MonoString* name)
{
	JSON_File* json = App->scene->GetJSONTool()->LoadJSON(LIBRARY_GAME_DATA);
	if (json == nullptr)
		json = App->scene->GetJSONTool()->CreateJSON(LIBRARY_GAME_DATA);

	if (json != nullptr)
	{
		const char* c_name = mono_string_to_utf8(name);

		const char* ret = json->GetString(c_name, "no_str");

		return mono_string_new(App->script_importer->GetDomain(), ret);
	}

	return mono_string_new(App->script_importer->GetDomain(), "no_str");
}

int NSScriptImporter::GetInt(MonoString * name)
{
	JSON_File* json = App->scene->GetJSONTool()->LoadJSON(LIBRARY_GAME_DATA);
	if (json == nullptr)
		json = App->scene->GetJSONTool()->CreateJSON(LIBRARY_GAME_DATA);

	if (json != nullptr)
	{
		const char* c_name = mono_string_to_utf8(name);

		int ret = json->GetNumber(c_name, 0);

		return ret;
	}

	return 0;
}

float NSScriptImporter::GetFloat(MonoString * name)
{
	JSON_File* json = App->scene->GetJSONTool()->LoadJSON(LIBRARY_GAME_DATA);
	if (json == nullptr)
		json = App->scene->GetJSONTool()->CreateJSON(LIBRARY_GAME_DATA);

	if (json != nullptr)
	{
		const char* c_name = mono_string_to_utf8(name);

		int ret = json->GetNumber(c_name, 0.0f);

		return ret;
	}

	return 0;
}

MonoObject * NSScriptImporter::ToQuaternion(MonoObject * object)
{
	if (!object)
	{
		return nullptr;
	}

	MonoClass* c = mono_object_get_class(object);
	MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
	MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
	MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

	float3 rotation;

	if (x_field) mono_field_get_value(object, x_field, &rotation.x);
	if (y_field) mono_field_get_value(object, y_field, &rotation.y);
	if (z_field) mono_field_get_value(object, z_field, &rotation.z);

	math::Quat quat = math::Quat::FromEulerXYZ(rotation.x * DEGTORAD, rotation.y * DEGTORAD, rotation.z * DEGTORAD);

	MonoClass* quaternion = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheQuaternion");
	if (quaternion)
	{
		MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), quaternion);
		if (new_object)
		{
			MonoClassField* x_field = mono_class_get_field_from_name(quaternion, "x");
			MonoClassField* y_field = mono_class_get_field_from_name(quaternion, "y");
			MonoClassField* z_field = mono_class_get_field_from_name(quaternion, "z");
			MonoClassField* w_field = mono_class_get_field_from_name(quaternion, "w");

			if (x_field) mono_field_set_value(new_object, x_field, &quat.x);
			if (y_field) mono_field_set_value(new_object, y_field, &quat.y);
			if (z_field) mono_field_set_value(new_object, z_field, &quat.z);
			if (w_field) mono_field_set_value(new_object, w_field, &quat.w);

			return new_object;
		}
	}
	return nullptr;
}

MonoObject * NSScriptImporter::ToEulerAngles(MonoObject * object)
{
	if (!object)
	{
		return nullptr;
	}

	MonoClass* c = mono_object_get_class(object);
	MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
	MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
	MonoClassField* z_field = mono_class_get_field_from_name(c, "z");
	MonoClassField* w_field = mono_class_get_field_from_name(c, "w");

	Quat rotation;

	if (x_field) mono_field_get_value(object, x_field, &rotation.x);
	if (y_field) mono_field_get_value(object, y_field, &rotation.y);
	if (z_field) mono_field_get_value(object, z_field, &rotation.z);
	if (w_field) mono_field_get_value(object, w_field, &rotation.w);

	math::float3 euler = rotation.ToEulerXYZ() * RADTODEG;

	MonoClass* vector = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
	if (vector)
	{
		MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), vector);
		if (new_object)
		{
			MonoClassField* x_field = mono_class_get_field_from_name(vector, "x");
			MonoClassField* y_field = mono_class_get_field_from_name(vector, "y");
			MonoClassField* z_field = mono_class_get_field_from_name(vector, "z");

			if (x_field) mono_field_set_value(new_object, x_field, &euler.x);
			if (y_field) mono_field_set_value(new_object, y_field, &euler.y);
			if (z_field) mono_field_set_value(new_object, z_field, &euler.z);

			return new_object;
		}
	}
	return nullptr;
}

MonoObject * NSScriptImporter::RotateTowards(MonoObject * current, MonoObject * target, float angle)
{
	MonoClass* c = mono_object_get_class(current);
	MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
	MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
	MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

	float3 from_vec;

	if (x_field) mono_field_get_value(current, x_field, &from_vec.x);
	if (y_field) mono_field_get_value(current, y_field, &from_vec.y);
	if (z_field) mono_field_get_value(current, z_field, &from_vec.z);

	MonoClass* c2 = mono_object_get_class(target);
	MonoClassField* x_field2 = mono_class_get_field_from_name(c2, "x");
	MonoClassField* y_field2 = mono_class_get_field_from_name(c2, "y");
	MonoClassField* z_field2 = mono_class_get_field_from_name(c2, "z");

	float3 to_vec;

	if (x_field) mono_field_get_value(target, x_field2, &to_vec.x);
	if (y_field) mono_field_get_value(target, y_field2, &to_vec.y);
	if (z_field) mono_field_get_value(target, z_field2, &to_vec.z);

	float rad_angle = angle * DEGTORAD;
	float3 final_vec;
	float3 final_cross;

	float3 to_vec2 = to_vec;
	float3 from_vec2 = from_vec;

	float3 ab_component = from_vec.Mul(to_vec).Div(to_vec.Mul(to_vec)).Mul(to_vec);
	float3 ortho_vec = from_vec - (from_vec.Mul(to_vec).Div(to_vec.Mul(to_vec))).Mul(to_vec);
	float3 w_vec = to_vec.Mul(ortho_vec);
	float x1 = math::Cos(rad_angle) / ortho_vec.Length();
	float x2 = math::Sin(rad_angle) / w_vec.Length();

	float3 ortho_angle = ortho_vec.Length() * (x1 * ortho_vec + x2 * w_vec);

	final_vec = ortho_angle + ab_component;

	to_vec2 = math::Cross(from_vec2, to_vec2);

	float3 ab_component2 = from_vec2.Mul(to_vec2).Div(to_vec2.Mul(to_vec2)).Mul(to_vec2);
	float3 ortho_vec2 = from_vec2 - (from_vec2.Mul(to_vec2).Div(to_vec2.Mul(to_vec2))).Mul(to_vec2);
	float3 w_vec2 = to_vec2.Mul(ortho_vec2);
	float x12 = math::Cos(rad_angle) / ortho_vec2.Length();
	float x22 = math::Sin(rad_angle) / w_vec2.Length();

	float3 ortho_angle2 = ortho_vec2.Length() * (x12 * ortho_vec2 + x22 * w_vec2);

	final_cross = ortho_angle2 + ab_component2;

	MonoClass* vector = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
	if (vector)
	{
		MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), vector);
		if (new_object)
		{
			MonoClassField* x_field = mono_class_get_field_from_name(vector, "x");
			MonoClassField* y_field = mono_class_get_field_from_name(vector, "y");
			MonoClassField* z_field = mono_class_get_field_from_name(vector, "z");

			if (x_field) mono_field_set_value(new_object, x_field, &final_vec.x);
			if (y_field) mono_field_set_value(new_object, y_field, &final_vec.y);
			if (z_field) mono_field_set_value(new_object, z_field, &final_vec.z);

			return new_object;
		}
	}

	return nullptr;
}

void NSScriptImporter::SetTimeScale(float scale)
{
	App->time->time_scale = scale;
}

float NSScriptImporter::GetTimeScale()
{
	return App->time->time_scale;
}

float NSScriptImporter::GetDeltaTime()
{
	return App->time->GetGameDt();
}

float NSScriptImporter::GetTimeSinceStart()
{
	return App->time->GetPlayTime();
}


void NSScriptImporter::Start(MonoObject * object, float time)
{
}

void NSScriptImporter::Stop(MonoObject * object)
{
}

float NSScriptImporter::ReadTime(MonoObject * object)
{
	return 0.0f;
}

mono_bool NSScriptImporter::IsKeyDown(MonoString * key_name)
{
	bool ret = false;
	const char* key = mono_string_to_utf8(key_name);
	SDL_Keycode code = App->input->StringToKey(key);
	if (code != SDL_SCANCODE_UNKNOWN)
	{
		if (App->input->GetKey(code) == KEY_DOWN) ret = true;
	}
	else
	{
		CONSOLE_WARNING("'%s' is not a key! Returned false by default", key);
	}

	return ret;
}

mono_bool NSScriptImporter::IsKeyUp(MonoString * key_name)
{
	bool ret = false;
	const char* key = mono_string_to_utf8(key_name);
	SDL_Keycode code = App->input->StringToKey(key);
	if (code != SDL_SCANCODE_UNKNOWN)
	{
		if (App->input->GetKey(code) == KEY_UP) ret = true;
	}
	else
	{
		CONSOLE_WARNING("'%s' is not a key! Returned false by default", key);
	}

	return ret;
}

mono_bool NSScriptImporter::IsKeyRepeat(MonoString * key_name)
{
	bool ret = false;
	const char* key = mono_string_to_utf8(key_name);
	SDL_Keycode code = App->input->StringToKey(key);
	if (code != SDL_SCANCODE_UNKNOWN)
	{
		if (App->input->GetKey(code) == KEY_REPEAT) ret = true;
	}
	else
	{
		CONSOLE_WARNING("'%s' is not a key! Returned false by default", key);
	}

	return ret;
}

mono_bool NSScriptImporter::IsMouseDown(int mouse_button)
{
	bool ret = false;
	if (mouse_button > 0 && mouse_button < 4)
	{
		if (App->input->GetMouseButton(mouse_button) == KEY_DOWN) ret = true;
	}
	else
	{
		CONSOLE_WARNING("%d is not a valid mouse button! Returned false by default");
	}

	return ret;
}

mono_bool NSScriptImporter::IsMouseUp(int mouse_button)
{
	bool ret = false;
	if (mouse_button > 0 && mouse_button < 4)
	{
		if (App->input->GetMouseButton(mouse_button) == KEY_UP) ret = true;
	}
	else
	{
		CONSOLE_WARNING("%d is not a valid mouse button! Returned false by default");
	}

	return ret;
}

mono_bool NSScriptImporter::IsMouseRepeat(int mouse_button)
{
	bool ret = false;
	if (mouse_button > 0 && mouse_button < 4)
	{
		if (App->input->GetMouseButton(mouse_button) == KEY_REPEAT) ret = true;
	}
	else
	{
		CONSOLE_WARNING("%d is not a valid mouse button! Returned false by default");
	}

	return ret;
}

MonoObject * NSScriptImporter::GetMousePosition()
{
	MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
	if (c)
	{
		MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
		if (new_object)
		{
			MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
			MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
			MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

			float mouse_x = App->input->GetMouseX();
			float mouse_y = App->input->GetMouseY();

			if (x_field) mono_field_set_value(new_object, x_field, &mouse_x);
			if (y_field) mono_field_set_value(new_object, y_field, &mouse_y);
			if (z_field) mono_field_set_value(new_object, z_field, 0);

			return new_object;
		}
	}
	return nullptr;
}

int NSScriptImporter::GetMouseXMotion()
{
	return App->input->GetMouseXMotion();
}

int NSScriptImporter::GetMouseYMotion()
{
	return App->input->GetMouseYMotion();
}

int NSScriptImporter::GetControllerJoystickMove(int pad, MonoString * axis)
{
	const char* key = mono_string_to_utf8(axis);
	JOYSTICK_MOVES code = App->input->StringToJoyMove(key);
	return App->input->GetControllerJoystickMove(pad, code);
}

int NSScriptImporter::GetControllerButton(int pad, MonoString * button)
{
	const char* key = mono_string_to_utf8(button);
	SDL_Keycode code = App->input->StringToKey(key);
	return App->input->GetControllerButton(pad, code);
}

void NSScriptImporter::RumbleController(int pad, float strength, int ms)
{
	App->input->RumbleController(pad, strength, ms);
}

void NSScriptImporter::CreateGameObject(MonoObject * object)
{
	/*if (!inside_function)
	{
		CONSOLE_ERROR("Can't create new GameObjects outside a function.");
		return;
	}*/
	GameObject* gameobject = App->scene->CreateGameObject();
	CreateGameObject(gameobject);
}

void NSScriptImporter::DestroyGameObject(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		App->script_importer->RemoveGameObjectInfoFromMono(go);

		go->SetActive(false);
		go->Destroy();
	}
}

void NSScriptImporter::DestroyGameObject(GameObject * go)
{
	if (go != nullptr)
	{
		for (std::list<Component*>::iterator it = go->components_list.begin(); it != go->components_list.end(); ++it)
		{
			RemoveComponentFromMonoObjectList(*it);
		}

		RemoveGameObjectFromMonoObjectList(go);

		for (std::vector<GameObject*>::iterator it = go->childs.begin(); it != go->childs.end(); ++it)
		{
			DestroyGameObject(*it);
		}
	}
}

MonoObject* NSScriptImporter::GetSelfGameObject()
{
	if (current_script)
	{
		return current_script->mono_self_object;
	}
	return nullptr;
}

void NSScriptImporter::SetGameObjectActive(MonoObject * object, mono_bool active)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		go->SetActive(active);
	}
}

mono_bool NSScriptImporter::GetGameObjectIsActive(MonoObject * object)
{
	GameObject* go = GetGameObjectFromMonoObject(object);

	if (go != nullptr)
	{
		return go->IsActive();
	}
	return false;
}

bool NSScriptImporter::IsMuted()
{
	return App->audio->IsMuted();
}

void NSScriptImporter::SetMute(bool set)
{
	App->audio->SetMute(set);
}

int NSScriptImporter::GetVolume()
{
	return App->audio->GetVolume();
}

void NSScriptImporter::SetVolume(MonoObject* obj, int volume)
{
	Component* comp = GetComponentFromMonoObject(obj);

	if (comp != nullptr)
	{
		ComponentAudioSource* as = (ComponentAudioSource*)comp;
		as->volume = volume;
	}
}

int NSScriptImporter::GetPitch()
{
	return App->audio->GetPitch();
}

void NSScriptImporter::SetPitch(int pitch)
{
	App->audio->SetPitch(pitch);
}

void NSScriptImporter::SetRTPCvalue(MonoString* name, float value)
{
	const char* new_name = mono_string_to_utf8(name);
	App->audio->SetRTPCvalue(new_name, value);
}

bool NSScriptImporter::Play(MonoObject * object, MonoString* name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentAudioSource* as = (ComponentAudioSource*)comp;
		const char* event_name = mono_string_to_utf8(name);
		return as->PlayEvent(event_name);
	}
	return false;
}

bool NSScriptImporter::Stop(MonoObject * object, MonoString* name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentAudioSource* as = (ComponentAudioSource*)comp;
		const char* event_name = mono_string_to_utf8(name);

		return as->StopEvent(event_name);
	}
	return false;
}

bool NSScriptImporter::Send(MonoObject * object, MonoString* name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentAudioSource* as = (ComponentAudioSource*)comp;
		const char* event_name = mono_string_to_utf8(name);

		return as->SendEvent(event_name);
	}
	return false;
}

bool NSScriptImporter::SetMyRTPCvalue(MonoObject * object, MonoString* name, float value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		const char* new_name = mono_string_to_utf8(name);
		ComponentAudioSource* as = (ComponentAudioSource*)comp;
		return as->obj->SetRTPCvalue(new_name, value);
	}
	return false;
}

bool NSScriptImporter::SetState(MonoObject* object, MonoString* group, MonoString* state)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		const char* group_name = mono_string_to_utf8(group);
		const char* state_name = mono_string_to_utf8(state);
		ComponentAudioSource* as = (ComponentAudioSource*)comp;
		as->SetState(group_name, state_name);
	}
	return true;
}

void NSScriptImporter::PlayEmmiter(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentParticleEmmiter* emmiter = (ComponentParticleEmmiter*)comp;

		if (emmiter != nullptr)
			emmiter->PlayEmmiter();
	}
}

void NSScriptImporter::StopEmmiter(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentParticleEmmiter* emmiter = (ComponentParticleEmmiter*)comp;

		if (emmiter != nullptr)
			emmiter->StopEmmiter();
	}
}

void NSScriptImporter::SetEmitterSpeed(MonoObject * object, float speed)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentParticleEmmiter* emmiter = (ComponentParticleEmmiter*)comp;

		if (emmiter != nullptr)
			emmiter->SetSpawnVelocity(speed);
	}
}

void NSScriptImporter::SetParticleSpeed(MonoObject * object, float speed)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentParticleEmmiter* emmiter = (ComponentParticleEmmiter*)comp;

		if (emmiter != nullptr)
			emmiter->SetParticlesVelocity(speed);
	}
}

void NSScriptImporter::SetLinearVelocity(MonoObject * object, float x, float y, float z)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRigidBody* rb = (ComponentRigidBody*)comp;

		if (rb != nullptr)
			rb->SetLinearVelocity({ x,y,z });
	}
}

void NSScriptImporter::SetAngularVelocity(MonoObject * object, float x, float y, float z)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRigidBody* rb = (ComponentRigidBody*)comp;

		if (rb != nullptr)
			rb->SetAngularVelocity({ x,y,z });
	}
}

void NSScriptImporter::AddTorque(MonoObject* object, float x, float y, float z, int force_type)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRigidBody* rb = (ComponentRigidBody*)comp;

		if (rb != nullptr)
		{
			float3 force = { x,y,z }; 
			rb->AddTorque(force, force_type);
		}			
	}
}

void NSScriptImporter::SetKinematic(MonoObject * object, bool kinematic)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRigidBody* rb = (ComponentRigidBody*)comp;

		if (rb != nullptr)
			rb->SetKinematic(kinematic);
	}
}

bool NSScriptImporter::IsKinematic(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRigidBody* rb = (ComponentRigidBody*)comp;

		if (rb != nullptr)
			return rb->IsKinematic();
	}

	return false; 
}

void NSScriptImporter::SetRBPosition(MonoObject * object, float x, float y, float z)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRigidBody* rb = (ComponentRigidBody*)comp;

		if (rb != nullptr)
			rb->SetPosition({ x,y,z });
	}
}

void NSScriptImporter::SetRBRotation(MonoObject * object, float x, float y, float z)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRigidBody* rb = (ComponentRigidBody*)comp;

		if (rb != nullptr)
		{
			Quat q = Quat::FromEulerXYZ(x, y, z);
			rb->SetRotation(q);
		}
	}
}

MonoObject* NSScriptImporter::GetRBPosition(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentRigidBody* rb = (ComponentRigidBody*)comp;

		if (rb != nullptr)
		{
			float3 pos = rb->GetPosition();

			MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
			if (c)
			{
				MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
				if (new_object)
				{
					MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
					MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
					MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

					ComponentTransform* transform = (ComponentTransform*)comp;

					if (x_field) mono_field_set_value(new_object, x_field, &pos.x);
					if (y_field) mono_field_set_value(new_object, y_field, &pos.y);
					if (z_field) mono_field_set_value(new_object, z_field, &pos.z);

					return new_object;
				}
			}
		}
	}
}

// ----- GOAP AGENT -----
mono_bool NSScriptImporter::GetBlackboardVariableB(MonoObject * object, MonoString * name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			bool var;
			const char* var_name = mono_string_to_utf8(name);
			if (goap->GetBlackboardVariable(var_name, var))
			{
				return var;
			}
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
	return false;
}

float NSScriptImporter::GetBlackboardVariableF(MonoObject * object, MonoString * name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			float var;
			const char* var_name = mono_string_to_utf8(name);
			if (goap->GetBlackboardVariable(var_name, var))
			{
				return var;
			}
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
	return 0.0f;
}

int NSScriptImporter::GetNumGoals(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			return goap->goals.size();
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
	return 0;
}

MonoString * NSScriptImporter::GetGoalName(MonoObject * object, int index)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			if (index >= 0 && index < goap->goals.size())
				mono_string_new(App->script_importer->GetDomain(), goap->goals[index]->GetName().c_str());
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
	return mono_string_new(App->script_importer->GetDomain(), "");
}

MonoString * NSScriptImporter::GetGoalConditionName(MonoObject * object, int index)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			if (index >= 0 && index < goap->goals.size())
				mono_string_new(App->script_importer->GetDomain(), goap->goals[index]->GetConditionName());
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
	return mono_string_new(App->script_importer->GetDomain(), "");
}

void NSScriptImporter::SetGoalPriority(MonoObject * object, int index, int priority)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			if (index >= 0 && index < goap->goals.size())
				goap->goals[index]->SetPriority(priority);
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
}

int NSScriptImporter::GetGoalPriority(MonoObject * object, int index)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			if (index >= 0 && index < goap->goals.size())
				return goap->goals[index]->GetPriority();
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
	return -1;
}

void NSScriptImporter::CompleteAction(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			goap->CompleteCurrentAction();
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
}

void NSScriptImporter::FailAction(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			goap->FailCurrentAction();
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
}

void NSScriptImporter::SetBlackboardVariable(MonoObject * object, MonoString * name, float value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			const char* var_name = mono_string_to_utf8(name);
			goap->SetBlackboardVariable(var_name, value);
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
}

void NSScriptImporter::SetBlackboardVariable(MonoObject * object, MonoString * name, bool value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		ComponentGOAPAgent* goap = (ComponentGOAPAgent*)comp;
		if (goap != nullptr)
		{
			const char* var_name = mono_string_to_utf8(name);
			goap->SetBlackboardVariable(var_name, value);
		}
		else
		{
			CONSOLE_WARNING("GOAPAgent not found!");
		}
	}
}
// ------

int NSScriptImporter::RandomInt(MonoObject * object)
{
	return App->RandomNumber().Int();
}

float NSScriptImporter::RandomFloat(MonoObject * object)
{
	return App->RandomNumber().Float();
}

float NSScriptImporter::RandomRange(float min, float max)
{
	return App->RandomNumber().FloatIncl(min, max);
}


void NSScriptImporter::LoadScene(MonoString * scene_name)
{
	const char* name = mono_string_to_utf8(scene_name);
	std::vector<std::string> scenes = App->resources->GetSceneList();
	for (std::string scene : scenes)
	{
		std::string scene_name = App->file_system->GetFileNameWithoutExtension(scene);
		if (scene_name == name)
		{
			App->scene->LoadScene(LIBRARY_SCENES_FOLDER + scene_name + ".jscene");
			return;
		}
	}

	CONSOLE_ERROR("LoadScene: Scene %s does not exist", name);
}

void NSScriptImporter::Quit()
{
	if (App->IsGame())
	{
		App->quit = true;
	}
	else
	{
		if (!App->IsStopped())
		{
			App->Stop();
		}
	}
}

void NSScriptImporter::SetBoolField(MonoObject * object, MonoString * field_name, bool value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				script->SetBoolProperty(name, value);
			}
		}
	}
}

bool NSScriptImporter::GetBoolField(MonoObject * object, MonoString * field_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				return script->GetBoolProperty(name);
			}
		}
	}
}

void NSScriptImporter::SetIntField(MonoObject * object, MonoString * field_name, int value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				script->SetIntProperty(name, value);
			}
		}
	}
}

int NSScriptImporter::GetIntField(MonoObject * object, MonoString * field_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				return script->GetIntProperty(name);
			}
		}
	}
}

void NSScriptImporter::SetFloatField(MonoObject * object, MonoString * field_name, float value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				script->SetFloatProperty(name, value);
			}
		}
	}
}

float NSScriptImporter::GetFloatField(MonoObject * object, MonoString * field_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				return script->GetFloatProperty(name);
			}
		}
	}
}

void NSScriptImporter::SetDoubleField(MonoObject * object, MonoString * field_name, double value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				script->SetDoubleProperty(name, value);
			}
		}
	}
}

double NSScriptImporter::GetDoubleField(MonoObject * object, MonoString * field_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				return script->GetDoubleProperty(name);
			}
		}
	}
}

void NSScriptImporter::SetStringField(MonoObject * object, MonoString * field_name, MonoString * value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			const char* value_to_string = mono_string_to_utf8(value);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				script->SetStringProperty(name, value_to_string);
			}
		}
	}
}

MonoString * NSScriptImporter::GetStringField(MonoObject * object, MonoString * field_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				std::string s = script->GetStringProperty(name);
				MonoString* ms = mono_string_new(App->script_importer->GetDomain(), s.c_str());
				return ms;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetGameObjectField(MonoObject * object, MonoString * field_name, MonoObject * value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				GameObject* go_to_set = nullptr;
				for (std::vector<GameObjectMono>::iterator it = created_gameobjects.begin(); it != created_gameobjects.end(); it++)
				{
					if ((*it).obj == value)
					{
						go_to_set = (*it).go;
						break;
					}
				}
				script->SetGameObjectProperty(name, go_to_set);
			}
		}
	}
}

MonoObject * NSScriptImporter::GetGameObjectField(MonoObject * object, MonoString * field_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				GameObject* go = script->GetGameObjectProperty(name);
				return GetMonoObjectFromGameObject(go);

			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetVector3Field(MonoObject * object, MonoString * field_name, MonoObject * value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				MonoClass* c_pos = mono_object_get_class(value);
				MonoClassField* x_field = mono_class_get_field_from_name(c_pos, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c_pos, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c_pos, "z");

				float3 vector;
				if (x_field) mono_field_get_value(value, x_field, &vector.x);
				if (y_field) mono_field_get_value(value, y_field, &vector.y);
				if (z_field) mono_field_get_value(value, z_field, &vector.z);

				script->SetVec3Property(name, vector);
			}
		}
	}
}

MonoObject * NSScriptImporter::GetVector3Field(MonoObject * object, MonoString * field_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				float3 vector3 = script->GetVec3Property(name);
				MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
				if (c)
				{
					MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
					if (new_object)
					{
						MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
						MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
						MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

						if (x_field) mono_field_set_value(new_object, x_field, &vector3.x);
						if (y_field) mono_field_set_value(new_object, y_field, &vector3.y);
						if (z_field) mono_field_set_value(new_object, z_field, &vector3.z);

						return new_object;
					}
				}
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetQuaternionField(MonoObject * object, MonoString * field_name, MonoObject * value)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				MonoClass* c_pos = mono_object_get_class(value);
				MonoClassField* x_field = mono_class_get_field_from_name(c_pos, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c_pos, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c_pos, "z");
				MonoClassField* w_field = mono_class_get_field_from_name(c_pos, "w");

				float4 quat;
				if (x_field) mono_field_get_value(value, x_field, &quat.x);
				if (y_field) mono_field_get_value(value, y_field, &quat.y);
				if (z_field) mono_field_get_value(value, z_field, &quat.z);
				if (w_field) mono_field_get_value(value, w_field, &quat.w);

				script->SetVec4Property(name, quat);
			}
		}
	}
}

MonoObject * NSScriptImporter::GetQuaternionField(MonoObject * object, MonoString * field_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(field_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				float4 quat = script->GetVec4Property(name);
				MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheQuaternion");
				if (c)
				{
					MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
					if (new_object)
					{
						MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
						MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
						MonoClassField* z_field = mono_class_get_field_from_name(c, "z");
						MonoClassField* w_field = mono_class_get_field_from_name(c, "w");

						if (x_field) mono_field_set_value(new_object, x_field, &quat.x);
						if (y_field) mono_field_set_value(new_object, y_field, &quat.y);
						if (z_field) mono_field_set_value(new_object, z_field, &quat.z);
						if (w_field) mono_field_set_value(new_object, w_field, &quat.w);

						return new_object;
					}
				}
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::CallFunction(MonoObject * object, MonoString * function_name)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp != nullptr)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;
			const char* name = mono_string_to_utf8(function_name);
			CSScript* script = (CSScript*)comp_script->GetScript();
			if (script)
			{
				/*NSScriptImporter* last_script = App->script_importer->GetCurrentSctipt();
				App->script_importer->SetCurrentScript(script);*/
				CONSOLE_LOG("function %s called from %s", name, script->GetName().c_str());
				MonoMethod* method = script->GetFunction(name, 0);
				if (method)
				{
					script->CallFunction(method, nullptr);
				}
				else
				{
					CONSOLE_ERROR("Function %s in script %s does not exist", name, script->GetName().c_str());
				}
				/*CONSOLE_LOG("Going back to %s", last_script->GetName().c_str());
				App->script_importer->SetCurrentScript(last_script);*/
			}
		}
	}
}

MonoObject* NSScriptImporter::CallFunctionArgs(MonoObject * object, MonoString * function_name, MonoArray* arr_args)
{
	MonoObject* ret = nullptr;

	std::string names = mono_string_to_utf8(function_name);

	if (names == "DamageSlaveOne")
	{
		int i = 0;

		i++;
	}

	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		if (comp->GetType() == Component::CompScript)
		{
			ComponentScript* comp_script = (ComponentScript*)comp;

			const char* name = mono_string_to_utf8(function_name);

			CSScript* script = (CSScript*)comp_script->GetScript();

			int args_count = 0;

			if(arr_args != nullptr)
				args_count = mono_array_length(arr_args);

			MonoMethod* method = script->GetFunction(name, args_count);

			if (method)
			{
				ret = script->CallFunctionArray(method, arr_args);
			}
		}
	}

	return ret;
}

MonoObject * NSScriptImporter::LoadPrefab(MonoString* prefab_name)
{
	const char* name = mono_string_to_utf8(prefab_name);

	Prefab* prefab = App->resources->GetPrefab(name);
	if (prefab != nullptr)
	{
		GameObject* go = App->scene->LoadPrefabToScene(prefab);

		if (go)
		{
			return App->script_importer->AddGameObjectInfoToMono(go);
		}
		
	}

	return nullptr;
}

void NSScriptImporter::Explosion(MonoObject * world_pos, float radius, float explosive_impulse)
{
	MonoClass* c = mono_object_get_class(world_pos);
	
	MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
	MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
	MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

	float3 pos;

	if (x_field) mono_field_get_value(world_pos, x_field, &pos.x);
	if (y_field) mono_field_get_value(world_pos, y_field, &pos.y);
	if (z_field) mono_field_get_value(world_pos, z_field, &pos.z);

	physx::PxVec3 phs_pos(pos.x, pos.y, pos.z);
	App->physics->Explode(phs_pos, radius, explosive_impulse);
}

MonoObject * NSScriptImporter::PhysicsRayCast(MonoObject * origin, MonoObject * direction, float max_distance)
{
	physx::PxVec3 origin_pos;
	physx::PxVec3 direction_pos;

	MonoClass* origin_class = mono_object_get_class(origin);
	MonoType* origin_type = mono_class_get_type(origin_class);
	std::string origin_name = mono_type_get_name(origin_type);
	if (origin_class && origin_name == "TheEngine.TheVector3")
	{
		MonoClassField* x_field = mono_class_get_field_from_name(origin_class, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(origin_class, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(origin_class, "z");

		if (x_field) mono_field_get_value(origin, x_field, &origin_pos.x);
		if (y_field) mono_field_get_value(origin, y_field, &origin_pos.y);
		if (z_field) mono_field_get_value(origin, z_field, &origin_pos.z);
	}

	MonoClass* direction_class = mono_object_get_class(direction);
	MonoType* direction_type = mono_class_get_type(direction_class);
	std::string direction_name = mono_type_get_name(direction_type);
	if (direction_class && direction_name == "TheEngine.TheVector3")
	{
		MonoClassField* x_field = mono_class_get_field_from_name(direction_class, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(direction_class, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(direction_class, "z");

		if (x_field) mono_field_get_value(direction, x_field, &direction_pos.x);
		if (y_field) mono_field_get_value(direction, y_field, &direction_pos.y);
		if (z_field) mono_field_get_value(direction, z_field, &direction_pos.z);
	}

	physx::PxReal distance = max_distance;

	RayCastInfo ray_info = App->physics->RayCast(origin_pos, direction_pos, distance);

	if (ray_info.colldier)
	{
		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheRayCastHit");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				mono_runtime_object_init(new_object);
				MonoClassField* contact_point_field = mono_class_get_field_from_name(c, "ContactPoint");
				MonoClassField* normal_field = mono_class_get_field_from_name(c, "Normal");
				MonoClassField* distance_field = mono_class_get_field_from_name(c, "Distance");
				MonoClassField* collider_field = mono_class_get_field_from_name(c, "Collider");

				if (contact_point_field)
				{
					MonoObject* contact_point_object = mono_field_get_value_object(App->script_importer->GetDomain(), contact_point_field, new_object);
					MonoClass* contact_point_class = mono_object_get_class(contact_point_object);

					MonoClassField* x_field = mono_class_get_field_from_name(contact_point_class, "x");
					MonoClassField* y_field = mono_class_get_field_from_name(contact_point_class, "y");
					MonoClassField* z_field = mono_class_get_field_from_name(contact_point_class, "z");

					mono_field_set_value(contact_point_object, x_field, &ray_info.position.x);
					mono_field_set_value(contact_point_object, y_field, &ray_info.position.y);
					mono_field_set_value(contact_point_object, z_field, &ray_info.position.z);
				}

				if (normal_field)
				{
					MonoObject* normal_object = mono_field_get_value_object(App->script_importer->GetDomain(), normal_field, new_object);
					MonoClass* normal_class = mono_object_get_class(normal_object);

					MonoClassField* x_field = mono_class_get_field_from_name(normal_class, "x");
					MonoClassField* y_field = mono_class_get_field_from_name(normal_class, "y");
					MonoClassField* z_field = mono_class_get_field_from_name(normal_class, "z");

					mono_field_set_value(normal_object, x_field, &ray_info.normal.x);
					mono_field_set_value(normal_object, y_field, &ray_info.normal.y);
					mono_field_set_value(normal_object, z_field, &ray_info.normal.z);

				}
				if (distance_field) mono_field_set_value(new_object, distance_field, &ray_info.distance);

				if (collider_field)
				{
					MonoObject* collider_object = nullptr;
					collider_object = GetMonoObjectFromComponent((Component*)ray_info.colldier);
					mono_field_set_value(new_object, collider_field, collider_object);
				}
				return new_object;
			}
		}
	}

	return nullptr;
}

MonoArray * NSScriptImporter::PhysicsRayCastAll(MonoObject * origin, MonoObject * direction, float max_distance)
{
	physx::PxVec3 origin_pos;
	physx::PxVec3 direction_pos;

	MonoClass* origin_class = mono_object_get_class(origin);
	MonoType* origin_type = mono_class_get_type(origin_class);
	std::string origin_name = mono_type_get_name(origin_type);
	if (origin_class && origin_name == "TheEngine.TheVector3")
	{
		MonoClassField* x_field = mono_class_get_field_from_name(origin_class, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(origin_class, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(origin_class, "z");

		if (x_field) mono_field_get_value(origin, x_field, &origin_pos.x);
		if (y_field) mono_field_get_value(origin, y_field, &origin_pos.y);
		if (z_field) mono_field_get_value(origin, z_field, &origin_pos.z);
	}

	MonoClass* direction_class = mono_object_get_class(direction);
	MonoType* direction_type = mono_class_get_type(direction_class);
	std::string direction_name = mono_type_get_name(direction_type);
	if (direction_class && direction_name == "TheEngine.TheVector3")
	{
		MonoClassField* x_field = mono_class_get_field_from_name(direction_class, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(direction_class, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(direction_class, "z");

		if (x_field) mono_field_get_value(direction, x_field, &direction_pos.x);
		if (y_field) mono_field_get_value(direction, y_field, &direction_pos.y);
		if (z_field) mono_field_get_value(direction, z_field, &direction_pos.z);
	}

	physx::PxReal distance = max_distance;

	std::vector<RayCastInfo> ray_info_list = App->physics->RayCastAll(origin_pos, direction_pos, distance);

	MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheRayCastHit");
	MonoArray* ray_hits = mono_array_new(App->script_importer->GetDomain(), c, ray_info_list.size());
	if (ray_hits)
	{
		int index = 0;
		for (RayCastInfo ray_info : ray_info_list)
		{
			if (ray_info.colldier)
			{
				if (c)
				{
					MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
					if (new_object)
					{
						mono_runtime_object_init(new_object);
						MonoClassField* contact_point_field = mono_class_get_field_from_name(c, "ContactPoint");
						MonoClassField* normal_field = mono_class_get_field_from_name(c, "Normal");
						MonoClassField* distance_field = mono_class_get_field_from_name(c, "Distance");
						MonoClassField* collider_field = mono_class_get_field_from_name(c, "Collider");

						if (contact_point_field)
						{
							MonoObject* contact_point_object = mono_field_get_value_object(App->script_importer->GetDomain(), contact_point_field, new_object);
							MonoClass* contact_point_class = mono_object_get_class(contact_point_object);

							MonoClassField* x_field = mono_class_get_field_from_name(contact_point_class, "x");
							MonoClassField* y_field = mono_class_get_field_from_name(contact_point_class, "y");
							MonoClassField* z_field = mono_class_get_field_from_name(contact_point_class, "z");

							mono_field_set_value(contact_point_object, x_field, &ray_info.position.x);
							mono_field_set_value(contact_point_object, y_field, &ray_info.position.y);
							mono_field_set_value(contact_point_object, z_field, &ray_info.position.z);
						}

						if (normal_field)
						{
							MonoObject* normal_object = mono_field_get_value_object(App->script_importer->GetDomain(), normal_field, new_object);
							MonoClass* normal_class = mono_object_get_class(normal_object);

							MonoClassField* x_field = mono_class_get_field_from_name(normal_class, "x");
							MonoClassField* y_field = mono_class_get_field_from_name(normal_class, "y");
							MonoClassField* z_field = mono_class_get_field_from_name(normal_class, "z");

							mono_field_set_value(normal_object, x_field, &ray_info.normal.x);
							mono_field_set_value(normal_object, y_field, &ray_info.normal.y);
							mono_field_set_value(normal_object, z_field, &ray_info.normal.z);

						}
						if (distance_field) mono_field_set_value(new_object, distance_field, &ray_info.distance);

						if (collider_field)
						{
							MonoObject* collider_object = nullptr;
							collider_object = GetMonoObjectFromComponent((Component*)ray_info.colldier);
							mono_field_set_value(new_object, collider_field, collider_object);
						}

						mono_array_set(ray_hits, MonoObject*, index, new_object);
						index++;
					}
				}
			}
		}
		return ray_hits;
	}

	return nullptr;
}

MonoObject * NSScriptImporter::ColliderGetGameObject(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		GameObject* go = comp->GetGameObject();
		return GetMonoObjectFromGameObject(go);
	}
	return nullptr;
}

MonoObject * NSScriptImporter::ColliderGetRigidBody(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		GameObject* go = comp->GetGameObject();
		ComponentRigidBody* rb = (ComponentRigidBody*)go->GetComponent(Component::CompRigidBody);
		if (rb)
		{
			return GetMonoObjectFromComponent(rb);
		}
	}
	return nullptr;
}

bool NSScriptImporter::ColliderIsTrigger(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;
		return col->IsTrigger();
	}
	return false;
}

void NSScriptImporter::ColliderSetTrigger(MonoObject * object, bool trigger)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;
		col->SetTrigger(trigger);
	}
}
MonoObject * NSScriptImporter::ClosestPoint(MonoObject * object, MonoObject * position)
{
	return nullptr;
}

MonoObject * NSScriptImporter::GetBoxColliderCenter(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				float3 center = col->GetColliderCenter();

				if (x_field) mono_field_set_value(new_object, x_field, &center.x);
				if (y_field) mono_field_set_value(new_object, y_field, &center.y);
				if (z_field) mono_field_set_value(new_object, z_field, &center.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetBoxColliderCenter(MonoObject * object, MonoObject * center)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		MonoClass* c = mono_object_get_class(center);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_center;

		if (x_field) mono_field_get_value(center, x_field, &new_center.x);
		if (y_field) mono_field_get_value(center, y_field, &new_center.y);
		if (z_field) mono_field_get_value(center, z_field, &new_center.z);

		col->SetColliderCenter(new_center);
	}
}

MonoObject * NSScriptImporter::GetBoxColliderSize(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				float3 size = col->GetBoxSize();

				if (x_field) mono_field_set_value(new_object, x_field, &size.x);
				if (y_field) mono_field_set_value(new_object, y_field, &size.y);
				if (z_field) mono_field_set_value(new_object, z_field, &size.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetBoxColliderSize(MonoObject * object, MonoObject * size)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		MonoClass* c = mono_object_get_class(size);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_size;

		if (x_field) mono_field_get_value(size, x_field, &new_size.x);
		if (y_field) mono_field_get_value(size, y_field, &new_size.y);
		if (z_field) mono_field_get_value(size, z_field, &new_size.z);

		col->SetBoxSize(new_size);
	}
}

MonoObject * NSScriptImporter::GetCapsuleColliderCenter(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				float3 center = col->GetColliderCenter();

				if (x_field) mono_field_set_value(new_object, x_field, &center.x);
				if (y_field) mono_field_set_value(new_object, y_field, &center.y);
				if (z_field) mono_field_set_value(new_object, z_field, &center.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetCapsuleColliderCenter(MonoObject * object, MonoObject * center)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		MonoClass* c = mono_object_get_class(center);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_center;

		if (x_field) mono_field_get_value(center, x_field, &new_center.x);
		if (y_field) mono_field_get_value(center, y_field, &new_center.y);
		if (z_field) mono_field_get_value(center, z_field, &new_center.z);

		col->SetColliderCenter(new_center);
	}
}

float NSScriptImporter::GetCapsuleColliderRadius(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->GetCapsuleRadius();
	}
	return 0.0f;
}

void NSScriptImporter::SetCapsuleColliderRadius(MonoObject * object, float radius)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->SetCapsuleRadius(radius);
	}
}

float NSScriptImporter::GetCapsuleColliderHeight(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->GetCapsuleHeight();
	}
	return 0.0f;
}

void NSScriptImporter::SetCapsuleColliderHeight(MonoObject * object, float height)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->SetCapsuleHeight(height);
	}
}

int NSScriptImporter::GetCapsuleColliderDirection(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->GetCapsuleDirection();
	}
	return -1;
}

void NSScriptImporter::SetCapsuleColliderDirection(MonoObject * object, int direction)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->SetCapsuleDirection((ComponentCollider::CapsuleDirection)direction);
	}
}

MonoObject * NSScriptImporter::GetSphereColliderCenter(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
		if (c)
		{
			MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
			if (new_object)
			{
				MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
				MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
				MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

				float3 center = col->GetColliderCenter();

				if (x_field) mono_field_set_value(new_object, x_field, &center.x);
				if (y_field) mono_field_set_value(new_object, y_field, &center.y);
				if (z_field) mono_field_set_value(new_object, z_field, &center.z);

				return new_object;
			}
		}
	}
	return nullptr;
}

void NSScriptImporter::SetSphereColliderCenter(MonoObject * object, MonoObject * center)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		MonoClass* c = mono_object_get_class(center);
		MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
		MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
		MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

		float3 new_center;

		if (x_field) mono_field_get_value(center, x_field, &new_center.x);
		if (y_field) mono_field_get_value(center, y_field, &new_center.y);
		if (z_field) mono_field_get_value(center, z_field, &new_center.z);

		col->SetColliderCenter(new_center);
	}
}

float NSScriptImporter::GetSphereColliderRadius(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->GetSphereRadius();
	}
	return 0.0f;
}

void NSScriptImporter::SetSphereColliderRadius(MonoObject * object, float radius)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->SetSphereRadius(radius);
	}
}

bool NSScriptImporter::GetMeshColliderConvex(MonoObject * object)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->IsConvex();
	}
	return false;
}

void NSScriptImporter::SetMeshColliderConvex(MonoObject * object, bool convex)
{
	Component* comp = GetComponentFromMonoObject(object);

	if (comp)
	{
		ComponentCollider* col = (ComponentCollider*)comp;

		return col->ChangeMeshToConvex(convex);
	}
}

void NSScriptImporter::DebugDrawLine(MonoObject * from, MonoObject * to, MonoObject * color)
{
	if (from && to && color)
	{
		float3 from_pos;
		float3 to_pos;
		float4 color_data;

		MonoClass* from_class = mono_object_get_class(from);
		if (from_class)
		{
			MonoClassField* x_field = mono_class_get_field_from_name(from_class, "x");
			MonoClassField* y_field = mono_class_get_field_from_name(from_class, "y");
			MonoClassField* z_field = mono_class_get_field_from_name(from_class, "z");

			if (x_field) mono_field_get_value(from, x_field, &from_pos.x);
			if (y_field) mono_field_get_value(from, y_field, &from_pos.y);
			if (z_field) mono_field_get_value(from, z_field, &from_pos.z);
		}

		MonoClass* to_class = mono_object_get_class(to);
		if (to_class)
		{
			MonoClassField* x_field = mono_class_get_field_from_name(to_class, "x");
			MonoClassField* y_field = mono_class_get_field_from_name(to_class, "y");
			MonoClassField* z_field = mono_class_get_field_from_name(to_class, "z");

			if (x_field) mono_field_get_value(to, x_field, &to_pos.x);
			if (y_field) mono_field_get_value(to, y_field, &to_pos.y);
			if (z_field) mono_field_get_value(to, z_field, &to_pos.z);
		}

		MonoClass* color_class = mono_object_get_class(color);
		if (color_class)
		{
			MonoClassField* r_field = mono_class_get_field_from_name(color_class, "r");
			MonoClassField* g_field = mono_class_get_field_from_name(color_class, "g");
			MonoClassField* b_field = mono_class_get_field_from_name(color_class, "b");
			MonoClassField* a_field = mono_class_get_field_from_name(color_class, "a");

			if (r_field) mono_field_get_value(color, r_field, &color_data.x);
			if (g_field) mono_field_get_value(color, g_field, &color_data.y);
			if (b_field) mono_field_get_value(color, b_field, &color_data.z);
			if (a_field) mono_field_get_value(color, a_field, &color_data.w);
		}

		App->renderer3D->debug_draw->Line(from_pos, to_pos, color_data);
	}
}

int NSScriptImporter::GetSizeX()
{
	int ret = 0;

	if (App->editor->game_window != nullptr)
	{
		App->editor->game_window->GetSize().x;
	}

	return ret;
}

int NSScriptImporter::GetSizeY()
{
	int ret = 0;

	if (App->editor->game_window != nullptr)
	{
		App->editor->game_window->GetSize().y;
	}

	return ret;
}

MonoObject* NSScriptImporter::WorldPosToScreenPos(MonoObject * world)
{
	MonoObject* ret = nullptr;

	float3 world_pos = float3::zero;

	if (world != nullptr)
	{
		MonoClass* from_class = mono_object_get_class(world);
		if (from_class)
		{
			MonoClassField* x_field = mono_class_get_field_from_name(from_class, "x");
			MonoClassField* y_field = mono_class_get_field_from_name(from_class, "y");
			MonoClassField* z_field = mono_class_get_field_from_name(from_class, "z");

			if (x_field) mono_field_get_value(world, x_field, &world_pos.x);
			if (y_field) mono_field_get_value(world, y_field, &world_pos.y);
			if (z_field) mono_field_get_value(world, z_field, &world_pos.z);

			if (App->renderer3D->game_camera != nullptr && App->editor->game_window != nullptr)
			{
				float3 camera_pos_normalized = float3::zero;

				camera_pos_normalized = App->renderer3D->game_camera->GetFrustum().Project(world_pos);

				float3 camera_pos = float3::zero;

				camera_pos.x = ((camera_pos_normalized.x + 1) / 2) * App->editor->game_window->GetSize().x;
				camera_pos.y = ((camera_pos_normalized.y + 1) / 2) * App->editor->game_window->GetSize().y;
				camera_pos.z = camera_pos_normalized.z;
						
				MonoClass* c = mono_class_from_name(App->script_importer->GetEngineImage(), "TheEngine", "TheVector3");
				if (c)
				{
					MonoObject* new_object = mono_object_new(App->script_importer->GetDomain(), c);
					if (new_object)
					{
						MonoClassField* x_field = mono_class_get_field_from_name(c, "x");
						MonoClassField* y_field = mono_class_get_field_from_name(c, "y");
						MonoClassField* z_field = mono_class_get_field_from_name(c, "z");

						if (x_field) mono_field_set_value(new_object, x_field, &camera_pos.x);
						if (y_field) mono_field_set_value(new_object, y_field, &camera_pos.y);
						if (z_field) mono_field_set_value(new_object, z_field, &camera_pos.z);

						return new_object;
					}
				}
			}
		}
	}

	return ret;
}


Component::ComponentType NSScriptImporter::CsToCppComponent(std::string component_type)
{
	Component::ComponentType type;

	if (component_type == "TheTransform")
	{
		type = Component::CompTransform;
	}
	else if (component_type == "TheFactory")
	{
		type = Component::CompFactory;
	}
	else if (component_type == "TheRectTransform")
	{
		type = Component::CompRectTransform;
	}
	else if (component_type == "TheProgressBar")
	{
		type = Component::CompProgressBar;
	}
	else if (component_type == "TheAudioSource")
	{
		type = Component::CompAudioSource;
	}
	else if (component_type == "TheParticleEmmiter")
	{
		type = Component::CompParticleSystem;
	}
	else if (component_type == "TheText")
	{
		type = Component::CompText;
	}
	else if (component_type == "TheRigidBody")
	{
		type = Component::CompRigidBody;
	}
	else if (component_type == "TheCollider")
	{
		type = Component::CompCollider;
	}
	else if (component_type == "TheBoxCollider")
	{
		type = Component::CompBoxCollider;
	}
	else if (component_type == "TheSphereCollider")
	{
		type = Component::CompSphereCollider;
	}
	else if (component_type == "TheCapsuleCollider")
	{
		type = Component::CompCapsuleCollider;
	}
	else if (component_type == "TheMeshCollider")
	{
		type = Component::CompMeshCollider;
	}
	else if (component_type == "TheGOAPAgent")
	{
		type = Component::CompGOAPAgent;
	}
	else if (component_type == "TheScript")
	{
		type = Component::CompScript;
	}
	else if (component_type == "TheLight")
	{
		type = Component::CompLight;
	}
	else if (component_type == "TheRadar")
	{
		type = Component::CompRadar;
	}
	else if (component_type == "TheCanvas")
	{
		type = Component::CompCanvas;
	}
	else
	{
		type = Component::CompUnknown;
	}
	return type;
}

std::string NSScriptImporter::CppComponentToCs(Component::ComponentType component_type)
{
	std::string cs_name;

	switch (component_type)
	{
	case Component::CompTransform:
		cs_name = "TheTransform";
		break;
	case Component::CompFactory:
		cs_name = "TheFactory";
		break;
	case Component::CompRectTransform:
		cs_name = "TheRectTransform";
		break;
	case Component::CompProgressBar:
		cs_name = "TheProgressBar";
		break;
	case Component::CompAudioSource:
		cs_name = "TheAudioSource";
		break;
	case Component::CompParticleSystem:
		cs_name = "TheParticleEmmiter";
		break;
	case Component::CompCanvas:
		cs_name = "TheCanvas";
		break;
	case Component::CompText:
		cs_name = "TheText";
		break;
	case Component::CompCollider:
		cs_name = "TheCollider";
		break;
	case Component::CompBoxCollider:
		cs_name = "TheBoxCollider";
		break;
	case Component::CompCapsuleCollider:
		cs_name = "TheCapsuleCollider";
		break;
	case Component::CompMeshCollider:
		cs_name = "TheMeshCollider";
		break;
	case Component::CompSphereCollider:
		cs_name = "TheSphereCollider";
		break;
	case Component::CompRigidBody:
		cs_name = "TheRigidBody";
		break;
	case Component::CompGOAPAgent:
		cs_name = "TheGOAPAgent";
		break;
	case Component::CompScript:
		cs_name = "TheScript";
		break;
	case Component::CompRadar:
		cs_name = "TheRadar";
		break;
	default:
		break;
	}

	return cs_name;
}
