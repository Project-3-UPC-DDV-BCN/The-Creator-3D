#ifndef _WWISE_H_
#define _WWISE_H_



#include <AK/SoundEngine/Common/AkTypes.h>
#include "Geomath.h"

namespace Wwise
{

	class SoundObject
	{
	public:

		SoundObject(unsigned long id, const char* n);
		~SoundObject();
		SoundObject* operator =(const SoundObject& so);

		unsigned long GetID();
		AkGameObjectID GetSoundID() const;
		const char* GetName();
		void SetPosition(float x = 0, float y = 0, float z = 0, float x_front = 1, float y_front = 0, float z_front = 0, float x_top = 0, float y_top = 1, float z_top = 0);
		AkVector GetPosition() const;
		AkVector GetTop() const;
		AkVector GetFront() const;
		
		void SetListener(unsigned long* id);

		void PlayEvent(unsigned long id);
		void PlayEvent(const char* name);

		void PlayMusic(unsigned long music_id);
		void PlayMusic(const char* music_name);
		bool SetRTPCvalue(const char * rtpc, float value);
		void SetAuxiliarySends(AkReal32 value, const char * target_bus, AkGameObjectID listener_id);

		void SetState(AkStateGroupID in_stateGroup, AkStateID 	in_state);
		void SetState(const char * in_stateGroup, const char * in_state);

	private:
		AkGameObjectID SoundID;
		const char* name = nullptr;
		AkVector position;
		AkVector top;
		AkVector front;
		
	};

	//Life cycle functions
	bool InitWwise(const char* language);
	bool InitMemSettings();
	bool InitStreamSettings();
	bool InitDeviceSettings();
	bool InitSoundEngine();
	bool InitMusicEngine();
	void ProcessAudio();
	bool CloseWwise();

	void SetDefaultListeners(unsigned long* id);
	void SetLanguage(const char* language);
	unsigned long LoadBank(const char* path);
	SoundObject* CreateSoundObj(unsigned long id, const char* name, float x, float y, float z, bool is_default_listener = false);
	void ChangeState(const char* group, const char* new_state);


	
}

#endif
