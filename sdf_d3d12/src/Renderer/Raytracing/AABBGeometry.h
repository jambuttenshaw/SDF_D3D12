#pragma once


/*
 *	Pure virtual base class describing the properties that AABB geometry needs to have
 */
class BaseAABBGeometry
{
public:
	BaseAABBGeometry() = default;
	virtual ~BaseAABBGeometry() = default;

	DISALLOW_COPY(BaseAABBGeometry)
	DEFAULT_MOVE(BaseAABBGeometry)

	// Getters
	virtual UINT GetAABBCount() const = 0;

	virtual D3D12_GPU_VIRTUAL_ADDRESS GetAABBBufferAddress() const = 0;
	static UINT GetAABBBufferStride() { return sizeof(D3D12_RAYTRACING_AABB); };
	virtual D3D12_GPU_VIRTUAL_ADDRESS GetPrimitiveDataBufferAddress() const = 0;
};


class AABBGeometryInstance
{
public:
	AABBGeometryInstance(const BaseAABBGeometry& geometry, D3D12_RAYTRACING_GEOMETRY_FLAGS flags, D3D12_GPU_DESCRIPTOR_HANDLE volumeSRV)
		: m_Geometry(&geometry)
		, m_Flags(flags)
		, m_VolumeSRV(volumeSRV)
	{}

	// Getters
	inline const BaseAABBGeometry& GetGeometry() const { return *m_Geometry; };
	inline D3D12_RAYTRACING_GEOMETRY_FLAGS GetFlags() const { return m_Flags; }

	inline void SetVolume(D3D12_GPU_DESCRIPTOR_HANDLE handle) { m_VolumeSRV = handle; }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetVolumeSRV() const { return m_VolumeSRV; }

private:
	const BaseAABBGeometry* m_Geometry = nullptr;
	D3D12_RAYTRACING_GEOMETRY_FLAGS m_Flags;

	// The volume to be rendered within this aabb geometry
	D3D12_GPU_DESCRIPTOR_HANDLE m_VolumeSRV;
};
