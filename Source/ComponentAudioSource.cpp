#include "ComponentAudioSource.h"
#include "Application.h"
#include "ModuleInput.h"
#include "ModuleAudio.h"
#include "AudioEvent.h"
#include "GameObject.h"
#include "ComponentTransform.h"
#include "ComponentListener.h"
#include "SoundBankResource.h"
#include "ModuleResources.h"

#include "../EngineResources/Editor_Settings/Wwise_IDs.h"

ComponentAudioSource::ComponentAudioSource(GameObject* attached_gameobject)
{
	SetActive(true);
	SetName("Audio Source");
	SetType(ComponentType::CompAudioSource);
	SetGameObject(attached_gameobject);

	ComponentTransform* trans = (ComponentTransform*) attached_gameobject->GetComponent(Component::CompTransform);

	obj = App->audio->CreateSoundObject(attached_gameobject->GetName().c_str(), trans->GetGlobalPosition());

	volume = DEFAULT_VOLUME;
	pitch = DEFAULT_PITCH;
}

ComponentAudioSource::~ComponentAudioSource()
{
	//App->audio->UnRegisterGO(id);
}

bool ComponentAudioSource::Update()
{
	bool ret = true;

	BROFILER_CATEGORY("Component - Audio Source - Update", Profiler::Color::Beige);

	ComponentListener* default_listener = App->audio->GetDefaultListener();
	if (default_listener != nullptr)
	{
		/*GameObject* listener_go = default_listener->GetGameObject();
		
		float distance_list_audio = (listener_go->GetGlobalPosition() - GetGameObject()->GetGlobalPosition()).Length();
		*/
		if (obj == nullptr)
			return ret;

		if (!muted) {
			obj->SetRTPCvalue("Volume", volume);
			obj->SetRTPCvalue("Pitch", pitch);
		}
		else {
			obj->SetRTPCvalue("Volume", 0);
		}

		ComponentTransform* trans = (ComponentTransform*)GetGameObject()->GetComponent(Component::CompTransform);

		if (trans)
		{
			float3 pos = trans->GetGlobalPosition();
			Quat rot = Quat::FromEulerXYZ(trans->GetGlobalRotation().x * DEGTORAD, trans->GetGlobalRotation().y * DEGTORAD, trans->GetGlobalRotation().z * DEGTORAD);

			float3 up = rot.Transform(float3(0, 1, 0));
			float3 front = rot.Transform(float3(0, 0, 1));

			up.Normalize();
			front.Normalize();

			obj->SetPosition(pos.x, pos.y, pos.z, front.x, front.y, front.z, up.x, up.y, up.z);
		}

		if (!events_to_play.empty()) {
			for (int i = 0; i < events_to_play.size(); i++) {
				PlayEvent(events_to_play[i]->name.c_str());
			}

			events_to_play.clear();
		}

	}
	else
		CONSOLE_WARNING("There is no listener, no sound will be reproduced");

	return ret;
}

void ComponentAudioSource::PlayMusic(double audio_id)
{
	AK::SoundEngine::PostEvent(audio_id, obj->GetID(), AK_EnableGetMusicPlayPosition);
}

void ComponentAudioSource::PlayMusic(const char * name)
{
	AK::SoundEngine::PostEvent(name, obj->GetID(), AK_EnableGetMusicPlayPosition);
}

void ComponentAudioSource::PlayEvent(uint id)
{
	AK::SoundEngine::PostEvent(id, obj->GetID());
}

bool ComponentAudioSource::PlayEvent(const char * event_name)
{
	if (obj != nullptr) 
	{
		obj->PlayEvent(event_name);
		return true;
		CONSOLE_DEBUG("Event played: %s", event_name);
	}
	else
	{
		CONSOLE_DEBUG("Nullptr");
		return false;
	}
}

bool ComponentAudioSource::StopEvent(const char * event_name)
{
	if (AK::SoundEngine::ExecuteActionOnEvent(event_name, AK::SoundEngine::AkActionOnEventType::AkActionOnEventType_Pause) == AKRESULT::AK_Success)
	{
		return true;
	}

	return false;
}

bool ComponentAudioSource::SendEvent(const char * name)
{
	for (int i = 0; i < events.size(); i++) {
		if (!strcmp(events[i]->name.c_str(), name)) {
			events_to_play.push_back(events[i]);
			return true;
		}
	}
	return false;
}

AkGameObjectID ComponentAudioSource::GetID() const
{
	return obj->GetSoundID();
}

void ComponentAudioSource::GetEvents()
{
	if (soundbank != nullptr) {
		for (int i = 0; i < soundbank->GetSoundBank()->events.size(); i++) {
			events.push_back(soundbank->GetSoundBank()->events[i]);
		}
	}
}

void ComponentAudioSource::ApplyReverb(float value, const char * bus)
{
	obj->SetAuxiliarySends(value, bus, App->audio->GetDefaultListener()->GetId());
}

void ComponentAudioSource::Save(Data & data) const
{
	data.AddInt("Type", GetType());
	data.AddBool("Active", IsActive());
	data.AddUInt("UUID", GetUID());
	data.AddInt("Sound ID", obj->GetID());
	if (soundbank != nullptr)
		data.AddString("SoundBank Name", soundbank->GetName());
}

void ComponentAudioSource::Load(Data & data)
{
	SetType((Component::ComponentType)data.GetInt("Type"));
	SetActive(data.GetBool("Active"));
	SetUID(data.GetUInt("UUID"));
	obj_to_load = data.GetInt("Sound ID");
	Wwise::SoundObject* tmp = App->audio->GetSoundObject(obj_to_load);
	if (tmp != nullptr)
		obj = tmp;
	soundbank = App->resources->GetSoundBank(data.GetString("SoundBank Name"));
}

int * ComponentAudioSource::GetPickedEventPtr()
{
	return &picked_event;
}

std::vector<AudioEvent*> ComponentAudioSource::GetEventsVector() const
{
	return events;
}

void ComponentAudioSource::ClearEventsVector()
{
	events.clear();
}

std::vector<AudioEvent*> ComponentAudioSource::GetEventsToPlayVector() const
{
	return events_to_play;
}

void ComponentAudioSource::StopAllEvents()
{
	for (int i = 0; i < soundbank->GetSoundBank()->events.size(); i++) {
		AK::SoundEngine::ExecuteActionOnEvent(soundbank->GetSoundBank()->events[i]->name.c_str(), AK::SoundEngine::AkActionOnEventType::AkActionOnEventType_Pause);
	}
}

void ComponentAudioSource::SetState(AkStateGroupID in_stateGroup, AkStateID in_state)
{
	obj->SetState(in_stateGroup, in_state);
}

void ComponentAudioSource::SetState(const char * in_stateGroup, const char * in_state)
{
	obj->SetState(in_stateGroup, in_state);
}
