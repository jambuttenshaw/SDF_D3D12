#pragma once


#include "SDFFactory.h"


class SDFFactoryAsync : public SDFFactory
{
public:
	SDFFactoryAsync();
	virtual ~SDFFactoryAsync();

	DISALLOW_COPY(SDFFactoryAsync)
	DEFAULT_MOVE(SDFFactoryAsync)

	virtual void BakeSDFSync(SDFObject* object, SDFEditList&& editList) override;
	void BakeSDFAsync(SDFObject* object, SDFEditList&& editList);

private:
	void AsyncFactoryThreadProc();

protected:
	std::unique_ptr<std::thread> m_FactoryThread;
	std::atomic<bool> m_TerminateThread = false;
	std::atomic<bool> m_AsyncInUse = false;

	struct BuildQueueItem
	{
		SDFObject* Object;
		std::unique_ptr<SDFEditList> EditList;
	};
	// Queue of bakes to perform
	std::deque<BuildQueueItem> m_BuildQueue;
	std::mutex m_QueueMutex;
};