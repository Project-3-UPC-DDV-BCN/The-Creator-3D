// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2016-2018 NVIDIA Corporation. All rights reserved.


#ifndef NVBLASTTKACTORIMPL_H
#define NVBLASTTKACTORIMPL_H


#include "NvBlastTkCommon.h"

#include "NvBlastAssert.h"
#include "NvBlastDLink.h"
#include "NvBlastIteratorBase.h"

#include "NvBlastTkJointImpl.h"

#include "NvBlast.h"

#include "NvBlastTkActor.h"

#include "PxFlags.h"

namespace Nv
{
namespace Blast
{

// Forward declarations:
class TkGroupImpl;
class TkFamilyImpl;
class TkAssetImpl;
class TkJointImpl;


/**
Struct-enum for actor status flags, used in TkGroup processing.
*/
struct TkActorFlag
{
	enum Enum
	{
		DAMAGED = (1 << 0),	//!< The actor had fractures applied successfully and will take the split step.
		PENDING = (1 << 1), //!< The actor will be processed when its group executes, used to update job queues when moving group.
	};
};


/**
Implementation of TkActor.
*/
class TkActorImpl : public TkActor
{
public:
	TkActorImpl();
	~TkActorImpl();

	// Begin TkActor
	virtual const NvBlastActor*	getActorLL() const override;

	virtual TkFamily&			getFamily() const override;

	virtual uint32_t			getIndex() const override;

	virtual TkGroup*			getGroup() const override;

	virtual TkGroup*			removeFromGroup() override;

	virtual const TkAsset*		getAsset() const override;

	virtual uint32_t 			getVisibleChunkCount() const override;

	virtual uint32_t 			getVisibleChunkIndices(uint32_t* visibleChunkIndices, uint32_t visibleChunkIndicesSize) const override;

	virtual uint32_t 			getGraphNodeCount() const override;

	virtual uint32_t 			getGraphNodeIndices(uint32_t* graphNodeIndices, uint32_t graphNodeIndicesSize) const override;

	virtual const float* 		getBondHealths() const override;

	virtual uint32_t			getSplitMaxActorCount() const override;

	virtual void				damage(const NvBlastDamageProgram& program, const void* programParams) override;

	virtual bool				isPending() const override;

	virtual void				generateFracture(NvBlastFractureBuffers* commands, const NvBlastDamageProgram& program, const void* programParams) const override;

	virtual void				applyFracture(NvBlastFractureBuffers* eventBuffers, const NvBlastFractureBuffers* commands) override;

	virtual uint32_t			getJointCount() const override;

	virtual uint32_t			getJoints(TkJoint** joints, uint32_t jointsSize) const override;

	virtual bool				isBoundToWorld() const override;
	// End TkActor

	// Begin TkObject
	virtual void				release() override;
	// End TkObject


	// Public methods
	
	/**
	Factory create method.

	\param[in]	desc	Actor descriptor set by the user.

	\return a pointer to a new TkActorImpl object if successful, NULL otherwise.
	*/
	static TkActorImpl*			create(const TkActorDesc& desc);

	/**
	TkActorImpl objects are created in an array within a TkFamilyImpl.  Actors may become
	'inactive' without their memory being freed.  If inactive, the actor should be treated as if
	it has been released.

	\return the active status of this TkActorImpl.
	*/
	bool						isActive() const;

	/**
	Utility to return the low-level family to which the low-level actor belongs.

	\return a pointer to the NvBlastFamily to which the low-level actor belongs.
	*/
	NvBlastFamily*				getFamilyLL() const;

	/**
	Utility to access the TkFamily to which this actor belongs.

	\return a reference to the TkFamilyImpl to which this TkActorImpl belongs.
	*/
	TkFamilyImpl&				getFamilyImpl() const;

	/**
	\return the index of this actor with its TkFamilyImpl.
	*/
	uint32_t					getIndexInternal() const;

	/**
	Access to the group to which this actor belongs, if any.

	\return a pointer to the TkGroupImpl to which this TkActorImpl belongs, if any.  If this actor is not in a group, this function returns NULL.
	*/
	TkGroupImpl*				getGroupImpl() const;

	/**
	Access to the low-level actor associated with this TkActorImpl.

	\return a pointer to the NvBlastActor associated with this TkActorImpl.  If this actor is inactive (see isActive), this function returns NULL.
	*/
	NvBlastActor*				getActorLLInternal() const;

	/**
	\return the number of TkJointImpl objects that reference this actor.
	*/
	uint32_t					getJointCountInternal() const;

	/**
		Joint iterator.  Usage:

		Given a TkActorImpl a,

		for (TkActorImpl::JointIt i(a); (bool)i; ++i)
		{
			TkJointImpl* joint = (TkJointImpl*)i;
			// ...
		}
	*/
	class JointIt : public DList::It
	{
	public:
		/** Constructed from an actor. */
		JointIt(const TkActorImpl& actor, Direction dir = Forward);

		/** Current joint. */
		TkJointImpl* operator * () const;
	};

	/**
	Implicit converter to TkActorData for events.
	*/
	operator Nv::Blast::TkActorData() const;

private:
	/**
	Functions to raise or check 'damaged' state: this actor will take the split step.
	'damaged' actors automatically become 'pending' also.
	*/
	void						markAsDamaged();
	bool						isDamaged() const;

	/**
	Raise actor to 'pending' state: this actor will be processed when its group executes next.
	Enqueues the actor in its group's job list if a group is set. Otherwise the group will enqueue the actor when it is added.
	*/
	void						makePending();

	/**
	Functions to add or remove an internal reference to a joint.  (Joints and actors mutually reference each other.)
	*/
	void						addJoint(TkJointLink& jointLink);
	void						removeJoint(TkJointLink& jointLink);

	struct DamageData
	{
		NvBlastDamageProgram program;
		const void*			 programParams;
	};

	// Data

	NvBlastActor*							m_actorLL;			  //!< The low-level actor associated with this actor
	TkFamilyImpl*							m_family;			  //!< The TkFamilyImpl to which this actor belongs
	TkGroupImpl*							m_group;			  //!< The TkGroupImpl (if any) to which this actor belongs
	uint32_t								m_groupJobIndex;	  //!< The index of this actor's job within its group's job list
	physx::PxFlags<TkActorFlag::Enum, char>	m_flags;			  //!< Status flags for this actor
	Array<DamageData>::type					m_damageBuffer;		  //!< Buffered damage input
	uint32_t								m_jointCount;		  //!< The number of joints referenced in m_jointList
	DList									m_jointList;		  //!< A doubly-linked list of joint references

//#if NV_PROFILE
	NvBlastTimers							m_timers;			//!< If profiling, each actor stores timing data
//#endif

	friend class TkWorker;					// m_damageBuffer and m_flags 
	friend class TkGroupImpl;
	friend class TkFamilyImpl;
	friend class TkJointImpl;
	friend class TkFrameworkImpl;
};


//////// TkActorImpl inline methods ////////

NV_INLINE TkFamilyImpl& TkActorImpl::getFamilyImpl() const
{
	NVBLAST_ASSERT(m_family != nullptr);

	return *m_family;
}


NV_INLINE uint32_t TkActorImpl::getIndexInternal() const
{
	NVBLAST_ASSERT(isActive());
	return NvBlastActorGetIndex(m_actorLL, logLL);
}


NV_INLINE NvBlastActor* TkActorImpl::getActorLLInternal() const
{
	return m_actorLL;
}


NV_INLINE uint32_t TkActorImpl::getJointCountInternal() const
{
	return m_jointCount;
}


NV_INLINE TkGroupImpl* TkActorImpl::getGroupImpl() const
{
	return m_group;
}


NV_INLINE bool TkActorImpl::isActive() const
{
	return m_actorLL != nullptr;
}


NV_INLINE bool TkActorImpl::isPending() const
{
	return m_flags.isSet(TkActorFlag::PENDING);
}


NV_INLINE void TkActorImpl::addJoint(TkJointLink& jointLink)
{
	NVBLAST_ASSERT(m_jointList.isSolitary(jointLink));

	m_jointList.insertHead(jointLink);
	++m_jointCount;
}


NV_INLINE void TkActorImpl::removeJoint(TkJointLink& jointLink)
{
	NVBLAST_ASSERT(!m_jointList.isSolitary(jointLink));
	NVBLAST_ASSERT(m_jointCount > 0);
	if (m_jointCount > 0)
	{
		--m_jointCount;
		m_jointList.remove(jointLink);
	}
}


//////// TkActorImpl::JointIt methods ////////

NV_INLINE TkActorImpl::JointIt::JointIt(const TkActorImpl& actor, Direction dir) : DList::It(actor.m_jointList, dir) {}


NV_INLINE TkJointImpl* TkActorImpl::JointIt::operator * () const
{
	const DLink* link = (const DLink*)(*this);
	return reinterpret_cast<const TkJointLink*>(link)->m_joint;
}

} // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTTKACTORIMPL_H
