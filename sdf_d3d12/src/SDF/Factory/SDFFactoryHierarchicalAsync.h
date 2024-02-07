#pragma once


#include "SDFFactoryHierarchical.h"
#include "Framework/GameTimer.h"


class SDFFactoryHierarchicalAsync : public SDFFactoryHierarchical
{
public:
	SDFFactoryHierarchicalAsync();
	virtual ~SDFFactoryHierarchicalAsync();

	DISALLOW_COPY(SDFFactoryHierarchicalAsync)
	DEFAULT_MOVE(SDFFactoryHierarchicalAsync)

	virtual void BakeSDFSync(SDFObject* object, SDFEditList&& editList) override;
	void BakeSDFAsync(SDFObject* object, SDFEditList&& editList);

	float GetAsyncBuildsPerSecond() const { return m_Timer.GetFPS(); };

private:
	void AsyncFactoryThreadProc();

protected:
	GameTimer m_Timer;

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
