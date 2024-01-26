#pragma once


class D3DDebugTools
{
public:
	// D3D12 Message handler
	static void D3DMessageHandler(D3D12_MESSAGE_CATEGORY Category, D3D12_MESSAGE_SEVERITY Severity, D3D12_MESSAGE_ID ID, LPCSTR pDescription, void* pContext);


private:
	static void LogAutoBreadcrumbs(const D3D12_AUTO_BREADCRUMB_NODE1* breadcrumb);
	static void LogPageFault(D3D12_DRED_PAGE_FAULT_OUTPUT* pageFault);
};
