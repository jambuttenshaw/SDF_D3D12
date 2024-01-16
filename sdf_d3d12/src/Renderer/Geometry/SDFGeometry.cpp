#include "pch.h"
#include "SDFGeometry.h"


SDFGeometry::SDFGeometry(UINT divisions, float halfSize)
	: AABBGeometry(divisions * divisions * divisions)
	, m_Divisions(divisions)
	, m_HalfSize(halfSize)
{
	const auto fDivisions = static_cast<float>(m_Divisions);
	const float invDivisions = 1.0f / fDivisions;

	for (UINT z = 0; z < m_Divisions; z++)
	for (UINT y = 0; y < m_Divisions; y++)
	for (UINT x = 0; x < m_Divisions; x++)
	{
		const auto fx = static_cast<float>(x);
		const auto fy = static_cast<float>(y);
		const auto fz = static_cast<float>(z);

		XMFLOAT3 uvwMin{
			invDivisions * fx,
			invDivisions * fy,
			invDivisions * fz
		};
		XMFLOAT3 uvwMax{
			uvwMin.x + invDivisions,
			uvwMin.y + invDivisions,
			uvwMin.z + invDivisions
		};

		float s = m_HalfSize / fDivisions;
		AddAABB(
			{
				2 * (fx - 0.5f * fDivisions + 0.5f) * s,
				2 * (fy - 0.5f * fDivisions + 0.5f) * s,
				2 * (fz - 0.5f * fDivisions + 0.5f) * s
			},
			{ s, s, s },
			uvwMin,
			uvwMax);
	}
}
