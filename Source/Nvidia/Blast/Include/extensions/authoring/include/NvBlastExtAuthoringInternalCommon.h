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
// Copyright (c) 2018 NVIDIA Corporation. All rights reserved.


#ifndef NVBLASTINTERNALCOMMON_H
#define NVBLASTINTERNALCOMMON_H
#include "NvBlastExtAuthoringTypes.h"
#include <algorithm>

using namespace physx;

namespace Nv
{
namespace Blast
{

/**
Edge representation with index of parent facet
*/
struct EdgeWithParent
{
	int32_t s, e; // Starting and ending vertices
	int32_t parent; // Parent facet index
	EdgeWithParent() : s(0), e(0), parent(0) {}
	EdgeWithParent(int32_t s, int32_t e, int32_t p) : s(s), e(e), parent(p) {}
};


/**
Comparator for sorting edges according to parent facet number.
*/
struct EdgeComparator
{
	bool operator()(const EdgeWithParent& a, const EdgeWithParent& b) const
	{
		if (a.parent == b.parent)
		{
			if (a.s == b.s)
			{
				return a.e < b.e;
			}
			else
			{
				return a.s < b.s;
			}
		}
		else
		{
			return a.parent < b.parent;
		}
	}
};


/**
Vertex projection direction flag.
*/
enum ProjectionDirections
{
	YZ_PLANE = 1 << 1,
	XY_PLANE = 1 << 2,
	ZX_PLANE = 1 << 3,

	OPPOSITE_WINDING = 1 << 4
};

/**
Computes best direction to project points.
*/
NV_FORCE_INLINE ProjectionDirections getProjectionDirection(const physx::PxVec3& normal)
{
	float maxv = std::max(std::abs(normal.x), std::max(std::abs(normal.y), std::abs(normal.z)));
	ProjectionDirections retVal;
	if (maxv == std::abs(normal.x))
	{
		retVal = YZ_PLANE;
		if (normal.x < 0) retVal = (ProjectionDirections)((int)retVal | (int)OPPOSITE_WINDING);
		return retVal;
	}
	if (maxv == std::abs(normal.y))
	{
		retVal = ZX_PLANE;
		if (normal.y > 0) retVal = (ProjectionDirections)((int)retVal | (int)OPPOSITE_WINDING);
		return retVal;
	}
	retVal = XY_PLANE;
	if (normal.z < 0) retVal = (ProjectionDirections)((int)retVal | (int)OPPOSITE_WINDING);
	return retVal;
}


/**
Computes point projected on given axis aligned plane.
*/
NV_FORCE_INLINE physx::PxVec2 getProjectedPoint(const physx::PxVec3& point, ProjectionDirections dir)
{
	if (dir & YZ_PLANE)
	{
		return physx::PxVec2(point.y, point.z);
	}
	if (dir & ZX_PLANE)
	{
		return physx::PxVec2(point.x, point.z);
	}
	return physx::PxVec2(point.x, point.y);
}

/**
Computes point projected on given axis aligned plane, this method is polygon-winding aware.
*/
NV_FORCE_INLINE physx::PxVec2 getProjectedPointWithWinding(const physx::PxVec3& point, ProjectionDirections dir)
{
	if (dir & YZ_PLANE)
	{
		if (dir & OPPOSITE_WINDING)
		{
			return physx::PxVec2(point.z, point.y);
		}
		else
		return physx::PxVec2(point.y, point.z);
	}
	if (dir & ZX_PLANE)
	{
		if (dir & OPPOSITE_WINDING)
		{
			return physx::PxVec2(point.z, point.x);
		}
		return physx::PxVec2(point.x, point.z);
	}
	if (dir & OPPOSITE_WINDING)
	{
		return physx::PxVec2(point.y, point.x);
	}
	return physx::PxVec2(point.x, point.y);
}



#define MAXIMUM_EXTENT 1000 * 1000 * 1000
#define BBOX_TEST_EPS 1e-5f 

/**
Test fattened bounding box intersetion.
*/
NV_INLINE bool  weakBoundingBoxIntersection(const physx::PxBounds3& aBox, const physx::PxBounds3& bBox)
{
	if (std::max(aBox.minimum.x, bBox.minimum.x) > std::min(aBox.maximum.x, bBox.maximum.x) + BBOX_TEST_EPS)
		return false;
	if (std::max(aBox.minimum.y, bBox.minimum.y) > std::min(aBox.maximum.y, bBox.maximum.y) + BBOX_TEST_EPS)
		return false;
	if (std::max(aBox.minimum.z, bBox.minimum.z) > std::min(aBox.maximum.z, bBox.maximum.z) + BBOX_TEST_EPS)
		return false;
	return true;
}



/**
Test segment vs plane intersection. If segment intersects the plane true is returned. Point of intersection is written into 'result'.
*/
NV_INLINE bool getPlaneSegmentIntersection(const PxPlane& pl, const PxVec3& a, const PxVec3& b, PxVec3& result)
{
	float div = (b - a).dot(pl.n);
	if (PxAbs(div) < 0.0001f)
	{
		if (pl.contains(a))
		{
			result = a;
			return true;
		}
		else
		{
			return false;
		}
	}
	float t = (-a.dot(pl.n) - pl.d) / div;
	if (t < 0.0f || t > 1.0f)
	{
		return false;
	}
	result = (b - a) * t + a;
	return true;
}


#define VEC_COMPARISON_OFFSET 1e-5f
/**
Vertex comparator for vertex welding.
*/
struct VrtComp
{
	bool operator()(const Vertex& a, const Vertex& b) const
	{
		if (a.p.x + VEC_COMPARISON_OFFSET < b.p.x) return true;
		if (a.p.x - VEC_COMPARISON_OFFSET > b.p.x) return false;
		if (a.p.y + VEC_COMPARISON_OFFSET < b.p.y) return true;
		if (a.p.y - VEC_COMPARISON_OFFSET > b.p.y) return false;
		if (a.p.z + VEC_COMPARISON_OFFSET < b.p.z) return true;
		if (a.p.z - VEC_COMPARISON_OFFSET > b.p.z) return false;

		if (a.n.x + 1e-3 < b.n.x) return true;
		if (a.n.x - 1e-3 > b.n.x) return false;
		if (a.n.y + 1e-3 < b.n.y) return true;
		if (a.n.y - 1e-3 > b.n.y) return false;
		if (a.n.z + 1e-3 < b.n.z) return true;
		if (a.n.z - 1e-3 > b.n.z) return false;


		if (a.uv[0].x + 1e-3 < b.uv[0].x) return true;
		if (a.uv[0].x - 1e-3 > b.uv[0].x) return false;
		if (a.uv[0].y + 1e-3 < b.uv[0].y) return true;
		return false;
	};
};

/**
Vertex comparator for vertex welding (not accounts normal and uv parameters of vertice).
*/
struct VrtPositionComparator
{
	bool operator()(const physx::PxVec3& a, const physx::PxVec3& b) const
	{
		if (a.x + VEC_COMPARISON_OFFSET < b.x) return true;
		if (a.x - VEC_COMPARISON_OFFSET > b.x) return false;
		if (a.y + VEC_COMPARISON_OFFSET < b.y) return true;
		if (a.y - VEC_COMPARISON_OFFSET > b.y) return false;
		if (a.z + VEC_COMPARISON_OFFSET < b.z) return true;
		if (a.z - VEC_COMPARISON_OFFSET > b.z) return false;
		return false;
	};
};

}	// namespace Blast
}	// namespace Nv

#endif