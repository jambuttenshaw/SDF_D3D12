#include "pch.h"
#include "SDFFactoryAsync.h"

#include "Renderer/D3DGraphicsContext.h"
#include "SDF/SDFEditList.h"
#include "SDF/SDFObject.h"

#include "pix3.h"
#include "Renderer/Buffer/ReadbackBuffer.h"


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
	if (object->GetResourcesState(SDFObject::RESOURCES_WRITE) == SDFObject::COMPUTING)
	{
		LOG_INFO("Compute in progress!");
		return;
	}

	{
		std::lock_guard lockGuard(m_QueueMutex);

		if (m_BuildQueue.empty())
		{
			LOG_INFO("Work pushed to queue");
			m_BuildQueue.push({ 
				object,
				std::make_unique<SDFEditList>(std::move(editList))
			});

			m_Complete = false;
		}
	}

	while (!m_Complete);
	LOG_INFO("Async work complete");
}


void SDFFactoryAsync::AsyncFactoryThreadProc()
{
	LOG_INFO("Async Factory Thread Begin");

	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();
	const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();

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
				LOG_WARN("Work acquired");
			}
		}

		if (object)
		{
			// We have an object to process
			while (true)
			{
				// Make sure the resources are available to write
				const auto resourceState = object->GetResourcesState(SDFObject::RESOURCES_WRITE);
				if (resourceState == SDFObject::SWITCHING)
				{
					// Wait until rendering queue is finished with the resources
					LOG_WARN("Waiting on render queue to finish with resources");
					directQueue->WaitForFenceCPUBlocking(directQueue->GetNextFenceValue() - 1);
					LOG_WARN("Render queue finished - resources ready for compute");
					break;
				}
				if (resourceState == SDFObject::READY_COMPUTE)
				{
					LOG_WARN("Resources ready for compute");
					break;
				}
			}
			PIXBeginEvent(PIX_COLOR_INDEX(24), L"Async Bake");

			object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTING);

			PIXBeginEvent(PIX_COLOR_INDEX(14), L"Wait for previous bake");
			computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);
			PIXEndEvent();

			PerformSDFBake_CPUBlocking(object, *editList.get());

			// The rendering queue should wait until these operations have completed
			directQueue->InsertWaitForQueue(computeQueue);

			// The CPU can optionally wait until the operations have completed too
			PIXBeginEvent(PIX_COLOR_INDEX(13), L"Wait for bake completion");
			computeQueue->WaitForFenceCPUBlocking(m_PreviousBakeFence);
			PIXEndEvent();

			LOG_WARN("Compute thread: work complete");

			object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTED);
			m_Complete = true;

			// Clear resources
			object = nullptr;
			editList = nullptr;

			PIXEndEvent();
		}

		// Yield to other threads
		SwitchToThread();
	}

	LOG_INFO("Async Factory Thread Termination");
}
