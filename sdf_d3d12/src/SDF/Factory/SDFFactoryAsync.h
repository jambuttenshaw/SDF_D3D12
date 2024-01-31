#pragma once


#include "SDFFactory.h"


class SDFFactoryAsync : SDFFactory
{
public:
	SDFFactoryAsync();
	virtual ~SDFFactoryAsync();

	DISALLOW_COPY(SDFFactoryAsync)
	DEFAULT_MOVE(SDFFactoryAsync)

	void BakeSDFAsync(SDFObject* object, SDFEditList&& editList);

private:
	void AsyncFactoryThreadProc();

protected:
	std::unique_ptr<std::thread> m_FactoryThread;
	std::atomic<bool> m_TerminateThread = false;

	struct BuildQueueItem
	{
		SDFObject* Object;
		std::unique_ptr<SDFEditList> EditList;
	};
	// Queue of bakes to perform
	std::queue<BuildQueueItem> m_BuildQueue;
	std::mutex m_QueueMutex;

};