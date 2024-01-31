#pragma once

#include "SDFFactory.h"


class SDFFactorySync : public SDFFactory
{
public:
	SDFFactorySync() = default;
	virtual ~SDFFactorySync() = default;

	DISALLOW_COPY(SDFFactorySync)
		DEFAULT_MOVE(SDFFactorySync)

		void BakeSDFSynchronous(SDFObject* object, const SDFEditList& editList, bool waitUntilComplete);

protected:
	// Synchronized Bake
	UINT64 m_PreviousBakeFence = 0;		// The fence value that will signal when the previous bake has completed
};

