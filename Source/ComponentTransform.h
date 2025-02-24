#pragma once
#include "Component.h"
#include "MathGeoLib\MathGeoLib.h"

class ComponentTransform :
	public Component
{
public:
	ComponentTransform(GameObject* attached_gameobject, bool is_particle = false);
	virtual ~ComponentTransform();

	void SetPosition(float3 position);
	void SetPositionFromRB(float3 position);
	float3 GetGlobalPosition() const;
	float3 GetLocalPosition() const;
	void SetRotation(float3 rotation);
	void SetIncrementalRotation(float3 rotation);
	float3 GetGlobalRotation() const;
	Quat GetGlobalQuatRotation() const;
	float3 GetLocalRotation() const;
	Quat GetQuatRotation() const;
	void SetQuatRotation(Quat q);
	void SetRotationFromRB(Quat q);
	void SetScale(float3 scale);
	float3 GetGlobalScale() const;
	float3 GetLocalScale() const;
	void UpdateGlobalMatrix(bool from_rigidbody = false);
	void UpdateLocals();
	const float4x4 GetMatrix() const;
	const float4x4 GetOpenGLMatrix() const;
	void SetMatrix(const float4x4 &matrix);
	void LookAt(float3 dir, float3 up);
	void LookAtY(float3 dir, float3 up);
	bool AnyDirty(); 

	void RotateAroundAxis(float3 axis, float angle);

	float3 GetForward()const;
	float3 GetUp()const;
	float3 GetRight()const;

	bool IsParticle(); 

	void Save(Data& data) const;
	void Load(Data& data);

	bool dirty; 

private:
	float3 position;
	Quat rotation;
	float3 shown_rotation;
	float3 scale;

	float3 global_pos;
	float3 global_rot;
	Quat global_quat_rot;
	float3 global_scale;

	float4x4 transform_matrix;

	bool is_particle;
};

