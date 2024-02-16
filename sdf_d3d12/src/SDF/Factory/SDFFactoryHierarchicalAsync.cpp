#include "pch.h"
#include "SDFFactoryHierarchicalAsync.h"

#include "Renderer/D3DGraphicsContext.h"
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

void SDFFactoryHierarchicalAsync::BakeSDFSync(const std::wstring& pipelineName, SDFObject* object, const SDFEditList& editList)
{
	if (m_AsyncInUse)
	{
		LOG_TRACE("Async compute in use - cannot perform sync bake.");
		return;
	}

	SDFFactoryHierarchical::BakeSDFSync(pipelineName, object, std::move(editList));
}


void SDFFactoryHierarchicalAsync::BakeSDFAsync(const std::wstring& pipelineName, SDFObject* object, const SDFEditList& editList)
{
	{
		std::lock_guard lockGuard(m_QueueMutex);

		// If this item has already been queued, then instead of queueing it again we can just update the arguments
		for (auto& item : m_BuildQueue)
		{
			if (item.Object == object)
			{
				item.PipelineName = pipelineName;
				item.EditList = editList;
				return;
			}
		}

		m_BuildQueue.push_back({ pipelineName, object, editList });
	}
}


void SDFFactoryHierarchicalAsync::AsyncFactoryThreadProc()
{
	LOG_INFO("Async Factory Thread Begin");
	PIXBeginEvent(PIX_COLOR_INDEX(51), L"Async Compute Thread");

	const auto computeQueue = g_D3DGraphicsContext->GetComputeCommandQueue();
	const auto directQueue = g_D3DGraphicsContext->GetDirectCommandQueue();

	m_Timer.Reset();

	std::wstring pipelineName;
	SDFObject* object = nullptr;
	SDFEditList editList(0); // max edits specified doesn't matter as this will be copy-constructed later

	while (!m_TerminateThread)
	{
		m_Timer.Tick();

		// Check for work
		{
			std::lock_guard lockGuard(m_QueueMutex);
			if (!m_BuildQueue.empty())
			{

				pipelineName = std::move(m_BuildQueue.front().PipelineName);
				object = m_BuildQueue.front().Object;
				editList = std::move(m_BuildQueue.front().EditList);

				m_BuildQueue.pop_front();
			}
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

			PerformSDFBake_CPUBlocking(pipelineName, object, editList);

			// The CPU can optionally wait until the operations have completed too
			PIXBeginEvent(PIX_COLOR_INDEX(13), L"Wait for bake completion");
			computeQueue->WaitForFenceCPUBlocking(m_PreviousWorkFence);
			PIXEndEvent();

			object->SetResourceState(SDFObject::RESOURCES_WRITE, SDFObject::COMPUTED);

			// Clear resources
			object = nullptr;
			editList.Reset();

			m_AsyncInUse = false;
			PIXEndEvent();
		}

		// Yield to other threads
		SwitchToThread();
	}

	PIXEndEvent();
	LOG_INFO("Async Factory Thread Terminated");
}
