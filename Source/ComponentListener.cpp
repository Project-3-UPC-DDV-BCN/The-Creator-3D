#include "ComponentListener.h"
#include "Application.h"
#include "ModuleAudio.h"
#include "AudioEvent.h"
#include "GameObject.h"
#include "ComponentTransform.h"

ComponentListener::ComponentListener(GameObject * attached_gameobject)
{
	SetActive(true);
	SetName("Audio Listener");
	SetType(ComponentType::CompAudioListener);
	SetGameObject(attached_gameobject);

	ComponentTransform* trans = (ComponentTransform*) attached_gameobject->GetComponent(Component::CompTransform);

	/*
	if (App->audio->GetDefaultListener() != nullptr)
	{
		this->obj = App->audio->CreateListener(attached_gameobject->GetName().c_str(), trans->GetGlobalPosition());
		App->audio->SetDefaultListener(this);
	}
	else {
		App->audio->SetListenerCreated(false);
	
	}
	*/
	
	if (App->audio->GetDefaultListener() == nullptr)
	{
		obj = App->audio->CreateListener(attached_gameobject->GetName().c_str(), trans->GetGlobalPosition());
	}
	else 
	{
		App->audio->SetListenerCreated(false);
		obj = App->audio->CreateListener(attached_gameobject->GetName().c_str(), trans->GetGlobalPosition());
	}
	
	App->audio->SetDefaultListener(this);
}

ComponentListener::~ComponentListener()
{
}

bool ComponentListener::Update()
{
	bool ret = true;
	BROFILER_CATEGORY("Component - Listener - Update", Profiler::Color::Beige);

	UpdatePosition();

	return ret;
}

void ComponentListener::UpdatePosition()
{
	ComponentTransform* trans = (ComponentTransform*)GetGameObject()->GetComponent(Component::ComponentType::CompTransform);

	if (trans != nullptr)
	{
		float3 pos = trans->GetGlobalPosition();
		Quat rot = Quat::FromEulerXYZ(trans->GetGlobalRotation().x * DEGTORAD, trans->GetGlobalRotation().y * DEGTORAD, trans->GetGlobalRotation().z * DEGTORAD);

		float3 up = rot.Transform(float3(0, 1, 0));
		float3 front = rot.Transform(float3(0, 0, 1));

		up.Normalize();
		front.Normalize();

		obj->SetPosition(pos.x, pos.y, pos.z, front.x, front.y, -front.z, up.x, up.y, up.z);

		box.minPoint = trans->GetGlobalPosition() - float3(1, 1, 1);
		box.maxPoint = trans->GetGlobalPosition() + float3(1, 1, 1);
	}
}

AkGameObjectID ComponentListener::GetId() const
{
	return obj->GetID();
}

void ComponentListener::ApplyReverb(float value, const char * bus)
{
	obj->SetAuxiliarySends(value, bus, App->audio->GetDefaultListener()->GetId());
}

void ComponentListener::Save(Data & data) const
{
	data.AddInt("Type", GetType());
	data.AddBool("Active", IsActive());
	data.AddUInt("UUID", GetUID());
	data.AddInt("Object ID", obj->GetID());
}

void ComponentListener::Load(Data & data)
{
	SetType((Component::ComponentType)data.GetInt("Type"));
	SetActive(data.GetBool("Active"));
	SetUID(data.GetUInt("UUID"));
	obj_to_load = data.GetInt("Object ID");
	if (obj_to_load != -1)
		obj = App->audio->GetSoundObject(obj_to_load);
}
