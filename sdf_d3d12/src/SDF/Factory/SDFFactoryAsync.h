#pragma once


#include "SDFFactorySync.h"


class SDFFactoryAsync : public SDFFactorySync
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
	std::deque<BuildQueueItem> m_BuildQueue;
	std::mutex m_QueueMutex;

	std::atomic<bool> m_Complete = false;

};