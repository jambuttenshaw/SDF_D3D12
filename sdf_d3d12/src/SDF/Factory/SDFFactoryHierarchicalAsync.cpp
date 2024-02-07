#include "pch.h"
#include "SDFFactoryHierarchicalAsync.h"

#include "Renderer/D3DGraphicsContext.h"
#include "SDF/SDFEditList.h"
#include "SDF/SDFObject.h"

#include "pix3.h"


SDFFactoryHierarchicalAsync::SDFFactoryHierarchicalAsync()
{
	m_FactoryThread = std::make_unique<std::thread>(&SDFFactoryHierarchicalAsync::AsyncFactoryThreadProc, this);
}


SDFFactoryHierarchicalAsync::~SDFFactoryHierarchicalAsync()
{
	if (m_FactoryThread)
	{
		m_TerminateThread = true;
		m_FactoryThread->join();
	}
}

void SDFFactoryHierarchicalAsync::BakeSDFSync(SDFObject* object, SDFEditList&& editList)
{
	if (m_AsyncInUse)
	{
		LOG_TRACE("Async compute in use - cannot perform sync bake.");
		return;
	}

	SDFFactoryHierarchical::BakeSDFSync(object, std::move(editList));
}


void SDFFactoryHierarchicalAsync::BakeSDFAsync(SDFObject* object, SDFEditList&& editList)
{
	{
		std::lock_guard lockGuard(m_QueueMutex);

		// If this item has already been queued, then instead of queueing it again we can just update the arguments
		for (auto& item : m_BuildQueue)
		{
			if (item.Object == object)
			{
				item.EditList = std::make_unique<SDFEditList>(std::move(editList));
				return;
			}
		}

		m_BuildQueue.push_back({
			object,
			std::make_unique<SDFEditList>(std::move(editList))
			});
	}
}


void SDFFactoryHierarchicalAsync::AsyncFactoryThreadProc()
{
	LOG_INFO("Async Factory Thread Begin");
	PIXBeginEvent(PIX_COLOR_INDEX(51), L"Async Compute Thread");

	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();
	const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();

	m_Timer.Reset();

	SDFObject* object = nullptr;
	std::unique_ptr<SDFEditList> editList;

	while (!m_TerminateThread)
	{
		m_Timer.Tick();

		// Check for work
		{
			std::lock_guard lockGuard(m_QueueMutex);
			PIXBeginEvent(PIX_COLOR_INDEX(52), L"Check for work");

			if (!m_BuildQueue.empty())
			{

				object = m_BuildQueue.front().Object;
				editList = std::move(m_BuildQueue.front().EditList);

				m_BuildQueue.pop_front();
			}

			PIXEndEvent();
		}

		if (object)
		{
			// We have an object to process
			PIXBeginEvent(PIX_COLOR_INDEX(53), L"Wait for resources");

			// Make sure the resources for each object in the pipe are available to write
			// When this fence value is reached, any resources being previously used by the GPU will now be available
			const UINT64 fenceValue = directQueue->GetNextFenceValue() - 1;
			while (!m_TerminateThread)
			{
				const auto resourceState = object->GetResourcesState(SDFObject::RESOURCES_WRITE);
				if (resourceState == SDFObject::SWITCHING)
				{
					// Wait until rendering queue is finished with the resources
					directQueue->WaitForFenceCPUBlocking(fenceValue);
					break;
				}
				if (resourceState == SDFObject::READY_COMPUTE)
				{
					break;
				}
			}
			PIXEndEvent();
			// Just in case application exists while we were waiting for resources to be ready
			if (m_TerminateThread)
				break;

			PIXBeginEvent(PIX_COLOR_INDEX(24), L"Async Bake");
			m_AsyncInUse = true;

			object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTING);

			PIXBeginEvent(PIX_COLOR_INDEX(14), L"Wait for previous bake");
			computeQueue->WaitForFenceCPUBlocking(m_PreviousWorkFence);
			PIXEndEvent();

			PerformSDFBake_CPUBlocking(object, *editList.get());

			// The CPU can optionally wait until the operations have completed too
			PIXBeginEvent(PIX_COLOR_INDEX(13), L"Wait for bake completion");
			computeQueue->WaitForFenceCPUBlocking(m_PreviousWorkFence);
			PIXEndEvent();

			object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTED);

			// Clear resources
			object = nullptr;
			editList.reset();

			m_AsyncInUse = false;
			PIXEndEvent();
		}

		// Yield to other threads
		SwitchToThread();
	}

	PIXEndEvent();
	LOG_INFO("Async Factory Thread Terminated");
}