#pragma once

#include "SDFFactory.h"


class SDFFactoryAsync : SDFFactory
{
public:
	SDFFactoryAsync();
	virtual ~SDFFactoryAsync();

	DISALLOW_COPY(SDFFactoryAsync)
	DEFAULT_MOVE(SDFFactoryAsync)

	void BakeSDFAsync(SDFObject* object, const SDFEditList& editList);

private:
	void FactoryAsyncProc();

protected:

};