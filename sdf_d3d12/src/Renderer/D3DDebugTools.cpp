#include "pch.h"
#include "D3DDebugTools.h"

#include "D3DGraphicsContext.h"


const char* BreadCrumbOpToStr(D3D12_AUTO_BREADCRUMB_OP op)
{
	switch (op)
	{
	case D3D12_AUTO_BREADCRUMB_OP_SETMARKER:										return "SETMARKER";
	case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT:										return "BEGINEVENT";
	case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT:											return "ENDEVENT";
	case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED:									return "DRAWINSTANCED";
	case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED:								return "DRAWINDEXEDINSTANCED";
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT:									return "EXECUTEINDIRECT";
	case D3D12_AUTO_BREADCRUMB_OP_DISPATCH:											return "DISPATCH";
	case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION:									return "COPYBUFFERREGION";
	case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION:								return "COPYTEXTUREREGION";
	case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE:										return "COPYRESOURCE";
	case D3D12_AUTO_BREADCRUMB_OP_COPYTILES:										return "COPYTILES";
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE:								return "RESOLVESUBRESOURCE";
	case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW:							return "CLEARRENDERTARGETVIEW";
	case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW:							return "CLEARUNORDEREDACCESSVIEW";
	case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW:							return "CLEARDEPTHSTENCILVIEW";
	case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER:									return "RESOURCEBARRIER";
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE:									return "EXECUTEBUNDLE";
	case D3D12_AUTO_BREADCRUMB_OP_PRESENT:											return "PRESENT";
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA:									return "RESOLVEQUERYDATA";
	case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION:									return "BEGINSUBMISSION";
	case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION:									return "ENDSUBMISSION";
	case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME:										return "DECODEFRAME";
	case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES:									return "PROCESSFRAMES";
	case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT:								return "ATOMICCOPYBUFFERUINT";
	case D3D12_AUTO_BREADCRUMB_OP_ATOMICCOPYBUFFERUINT64:							return "ATOMICCOPYBUFFERUINT64";
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCEREGION:							return "RESOLVESUBRESOURCEREGION";
	case D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE:								return "WRITEBUFFERIMMEDIATE";
	case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME1:										return "DECODEFRAME1";
	case D3D12_AUTO_BREADCRUMB_OP_SETPROTECTEDRESOURCESESSION:						return "SETPROTECTEDRESOURCESESSION";
	case D3D12_AUTO_BREADCRUMB_OP_DECODEFRAME2:										return "DECODEFRAME2";
	case D3D12_AUTO_BREADCRUMB_OP_PROCESSFRAMES1:									return "PROCESSFRAMES1";
	case D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE:				return "BUILDRAYTRACINGACCELERATIONSTRUCTURE";
	case D3D12_AUTO_BREADCRUMB_OP_EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO:	return "EMITRAYTRACINGACCELERATIONSTRUCTUREPOSTBUILDINFO";
	case D3D12_AUTO_BREADCRUMB_OP_COPYRAYTRACINGACCELERATIONSTRUCTURE:				return "COPYRAYTRACINGACCELERATIONSTRUCTURE";
	case D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS:										return "DISPATCHRAYS";
	case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEMETACOMMAND:							return "INITIALIZEMETACOMMAND";
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEMETACOMMAND:								return "EXECUTEMETACOMMAND";
	case D3D12_AUTO_BREADCRUMB_OP_ESTIMATEMOTION:									return "ESTIMATEMOTION";
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVEMOTIONVECTORHEAP:							return "RESOLVEMOTIONVECTORHEAP";
	case D3D12_AUTO_BREADCRUMB_OP_SETPIPELINESTATE1:								return "SETPIPELINESTATE1";
	case D3D12_AUTO_BREADCRUMB_OP_INITIALIZEEXTENSIONCOMMAND:						return "INITIALIZEEXTENSIONCOMMAND";
	case D3D12_AUTO_BREADCRUMB_OP_EXECUTEEXTENSIONCOMMAND:							return "EXECUTEEXTENSIONCOMMAND";
	case D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH:										return "DISPATCHMESH";
	case D3D12_AUTO_BREADCRUMB_OP_ENCODEFRAME:										return "ENCODEFRAME";
	case D3D12_AUTO_BREADCRUMB_OP_RESOLVEENCODEROUTPUTMETADATA:						return "RESOLVEENCODEROUTPUTMETADATA";
	case D3D12_AUTO_BREADCRUMB_OP_BARRIER:											return "BARRIER";
	}
	return "UNKNOWN";
}

const char* AllocationTypeToStr(D3D12_DRED_ALLOCATION_TYPE allocation)
{
	switch (allocation)
	{
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_QUEUE:								return "COMMAND_QUEUE";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_ALLOCATOR:							return "COMMAND_ALLOCATOR";
		case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_STATE:								return "PIPELINE_STATE";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_LIST:								return "COMMAND_LIST";
		case D3D12_DRED_ALLOCATION_TYPE_FENCE:										return "FENCE";
		case D3D12_DRED_ALLOCATION_TYPE_DESCRIPTOR_HEAP:							return "DESCRIPTOR_HEAP";
		case D3D12_DRED_ALLOCATION_TYPE_HEAP:										return "HEAP";
		case D3D12_DRED_ALLOCATION_TYPE_QUERY_HEAP:									return "QUERY_HEAP";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_SIGNATURE:							return "COMMAND_SIGNATURE";
		case D3D12_DRED_ALLOCATION_TYPE_PIPELINE_LIBRARY:							return "PIPELINE_LIBRARY";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER:								return "VIDEO_DECODER";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_PROCESSOR:							return "VIDEO_PROCESSOR";
		case D3D12_DRED_ALLOCATION_TYPE_RESOURCE:									return "RESOURCE";
		case D3D12_DRED_ALLOCATION_TYPE_PASS:										return "PASS";
		case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSION:								return "CRYPTOSESSION";
		case D3D12_DRED_ALLOCATION_TYPE_CRYPTOSESSIONPOLICY:						return "CRYPTOSESSIONPOLICY";
		case D3D12_DRED_ALLOCATION_TYPE_PROTECTEDRESOURCESESSION:					return "PROTECTEDRESOURCESESSION";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_DECODER_HEAP:							return "VIDEO_DECODER_HEAP";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_POOL:								return "COMMAND_POOL";
		case D3D12_DRED_ALLOCATION_TYPE_COMMAND_RECORDER:							return "COMMAND_RECORDER";
		case D3D12_DRED_ALLOCATION_TYPE_STATE_OBJECT:								return "STATE_OBJECT";
		case D3D12_DRED_ALLOCATION_TYPE_METACOMMAND:								return "METACOMMAND";
		case D3D12_DRED_ALLOCATION_TYPE_SCHEDULINGGROUP:							return "SCHEDULINGGROUP";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_ESTIMATOR:						return "VIDEO_MOTION_ESTIMATOR";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_MOTION_VECTOR_HEAP:					return "VIDEO_MOTION_VECTOR_HEAP";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_EXTENSION_COMMAND:					return "VIDEO_EXTENSION_COMMAND";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER:								return "VIDEO_ENCODER";
		case D3D12_DRED_ALLOCATION_TYPE_VIDEO_ENCODER_HEAP:							return "VIDEO_ENCODER_HEAP";
		case D3D12_DRED_ALLOCATION_TYPE_INVALID:									return "INVALID";
	}
	return "UNKNOWN";
}



void D3DDebugTools::D3DMessageHandler(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext)
{
	switch (Severity)
	{
	case D3D12_MESSAGE_SEVERITY_CORRUPTION:
		D3D_LOG_FATAL(pDescription); break;
	case D3D12_MESSAGE_SEVERITY_ERROR:
		D3D_LOG_ERROR(pDescription); break;
	case D3D12_MESSAGE_SEVERITY_WARNING:
		D3D_LOG_WARN(pDescription); break;
	case D3D12_MESSAGE_SEVERITY_INFO:
		D3D_LOG_INFO(pDescription); break;
	case D3D12_MESSAGE_SEVERITY_MESSAGE:
	default:
		D3D_LOG_TRACE(pDescription); break;
	}

	if (g_D3DGraphicsContext && g_D3DGraphicsContext->CheckDeviceRemovedStatus())
	{
		// Attempt to recover DRED
		ComPtr<ID3D12DeviceRemovedExtendedData1> pDred;
		THROW_IF_FAIL(g_D3DGraphicsContext->GetDevice()->QueryInterface(IID_PPV_ARGS(&pDred)));

		D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 DredAutoBreadcrumbsOutput;
		D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
		THROW_IF_FAIL(pDred->GetAutoBreadcrumbsOutput1(&DredAutoBreadcrumbsOutput));
		THROW_IF_FAIL(pDred->GetPageFaultAllocationOutput(&DredPageFaultOutput));

		if (DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode)
		{
			LogAutoBreadcrumbs(DredAutoBreadcrumbsOutput.pHeadAutoBreadcrumbNode);
		}
		if (DredPageFaultOutput.PageFaultVA)
		{
			LogPageFault(&DredPageFaultOutput);
		}
	}

}


void D3DDebugTools::LogAutoBreadcrumbs(const D3D12_AUTO_BREADCRUMB_NODE1* breadcrumb)
{
	LOG_INFO("DRED Auto Breadcrumbs:");

	while (breadcrumb)
	{
		const UINT lastBreadcrumb = breadcrumb->pLastBreadcrumbValue ? *breadcrumb->pLastBreadcrumbValue : 0;
		if (breadcrumb->BreadcrumbCount && breadcrumb->BreadcrumbCount > lastBreadcrumb)
		{
			if (breadcrumb->pCommandQueueDebugNameW)
				LOG_TRACE(L"Command Queue: {}", breadcrumb->pCommandQueueDebugNameW);
			if (breadcrumb->pCommandListDebugNameW)
				LOG_TRACE(L"Command List: {}", breadcrumb->pCommandListDebugNameW);

			if (breadcrumb->pCommandHistory)
			{
				LOG_TRACE("Operation History:");
				auto command = breadcrumb->pCommandHistory;
				for (UINT i = 0; i < breadcrumb->BreadcrumbCount && command; i++)
				{
					if (i == lastBreadcrumb)
					{
						LOG_WARN("--->{}", BreadCrumbOpToStr(*command));
					}
					else
					{
						LOG_TRACE("    {}", BreadCrumbOpToStr(*command));
					}
					command++;
				}
			}
			LOG_INFO("-------------"); // separator
		}

		breadcrumb = breadcrumb->pNext;
	}
}


void D3DDebugTools::LogPageFault(D3D12_DRED_PAGE_FAULT_OUTPUT* pageFault)
{
	LOG_INFO("Page Fault VA: {}", pageFault->PageFaultVA);

	LOG_INFO("Existing objects with matching VA:");
	auto allocation = pageFault->pHeadExistingAllocationNode;
	while (allocation)
	{
		LOG_TRACE(L"	Name: {}", allocation->ObjectNameW);
		LOG_TRACE("	Type: {}", AllocationTypeToStr(allocation->AllocationType));
		LOG_INFO("    -------------");
		allocation = allocation->pNext;
	}

	LOG_INFO("Recently freed objects with matching VA:");
	allocation = pageFault->pHeadRecentFreedAllocationNode;
	while (allocation)
	{
		LOG_TRACE(L"	Name: {}", allocation->ObjectNameW);
		LOG_TRACE("	Type: {}", AllocationTypeToStr(allocation->AllocationType));
		LOG_INFO("    -------------");
		allocation = allocation->pNext;
	}
}
