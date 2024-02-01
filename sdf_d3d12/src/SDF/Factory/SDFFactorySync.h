#pragma once

#include "SDFFactory.h"


class SDFFactorySync : public SDFFactory
{
public:
	SDFFactorySync() = default;
	virtual ~SDFFactorySync() = default;

	DISALLOW_COPY(SDFFactorySync)
	DEFAULT_MOVE(SDFFactorySync)

	void BakeSDFSynchronous(SDFObject* object, const SDFEditList& editList);
};
