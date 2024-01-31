#include "pch.h"
#include "SDFFactoryAsync.h"

#include "Renderer/D3DGraphicsContext.h"
#include "SDF/SDFEditList.h"
#include "SDF/SDFObject.h"


SDFFactoryAsync::SDFFactoryAsync()
{
	m_FactoryThread = std::make_unique<std::thread>(&SDFFactoryAsync::AsyncFactoryThreadProc, this);
}


SDFFactoryAsync::~SDFFactoryAsync()
{
	if (m_FactoryThread)
	{
		m_TerminateThread = true;
		m_FactoryThread->join();
	}
}

void SDFFactoryAsync::BakeSDFAsync(SDFObject* object, SDFEditList&& editList)
{
	std::lock_guard lockGuard(m_QueueMutex);

	m_BuildQueue.push({ 
		object,
		std::make_unique<SDFEditList>(std::move(editList))
	});
}


void SDFFactoryAsync::AsyncFactoryThreadProc()
{
	LOG_INFO("Async Factory Thread Begin");

	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();


	while (!m_TerminateThread)
	{
		SDFObject* object = nullptr;
		std::unique_ptr<SDFEditList> editList;

		// Check for work
		{
			std::lock_guard lockGuard(m_QueueMutex);
			if (!m_BuildQueue.empty())
			{
				object = m_BuildQueue.front().Object;
				editList = std::move(m_BuildQueue.front().EditList);
				m_BuildQueue.pop();
			}
		}

		if (object)
		{
			// We have an object to process
			while (true)
			{
				// Make sure the resources are available to write
				const auto resourceState = object->GetResourcesState(SDFObject::RESOURCES_WRITE);
				if (resourceState == SDFObject::READY_COMPUTE)
				{
					break;
				}
			}

			object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTING);

			PerformSDFBake_CPUBlocking(object, *editList);

			// Wait until bake is finished
			computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);

			// Signal that bake has completed
			object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTED);
		}

		// Yield to other threads
		SwitchToThread();
	}

	LOG_INFO("Async Factory Thread Termination");
}

