#include "pch.h"
#include "Demos.h"

#include "Framework/Math.h"


struct DropsDemoData
{
	DropsDemoData()
	{
		Random::Seed(0);
		for (UINT i = 0; i < SphereCount; i++)
		{
			Spheres.push_back({});
			SphereData& sphereData = Spheres.at(i);

			sphereData.radius = Random::Float(0.25f, 0.75f);
			sphereData.scale = {
				Random::Float(-2.0f, 2.0f),
				Random::Float(-5.0f, 5.0f),
				Random::Float(-2.0f, 2.0f)
			};
			sphereData.speed = {
				Random::Float(-1.0f, 1.0f) / sphereData.radius,
				Random::Float(-1.0f, 1.0f) / sphereData.radius,
				Random::Float(-1.0f, 1.0f) / sphereData.radius
			};
			sphereData.offset = {
				Random::Float(-3.0f, 3.0f),
				0.0f,
				Random::Float(-3.0f, 3.0f)
			};
		}
	}

	struct SphereData
	{
		XMFLOAT3 scale;
		XMFLOAT3 speed;
		XMFLOAT3 offset;
		float radius;
	};
	UINT SphereCount = 256;
	std::vector<SphereData> Spheres;
	float SphereBlend = 0.5f;
	float Time = 0.0f;
};


SDFEditList Demos::DropsDemo(float deltaTime)
{
	static DropsDemoData data;
	data.Time += deltaTime;

	SDFEditList editList(data.SphereCount + 2, 8.0f);

	// Create base
	editList.AddEdit(SDFEdit::CreateBox({}, { 6.0f, 0.05f, 6.0f }));

	for (UINT i = 0; i < data.SphereCount; i++)
	{
		Transform transform;
		transform.SetTranslation(
			{
				data.Spheres.at(i).offset.x + data.Spheres.at(i).scale.x * cosf(data.Spheres.at(i).speed.x * data.Time),
				data.Spheres.at(i).offset.y + data.Spheres.at(i).scale.y * cosf(data.Spheres.at(i).speed.y * data.Time),
				data.Spheres.at(i).offset.z + data.Spheres.at(i).scale.z * cosf(data.Spheres.at(i).speed.z * data.Time)
			});
		editList.AddEdit(SDFEdit::CreateSphere(transform, data.Spheres.at(i).radius, SDF_OP_SMOOTH_UNION, data.SphereBlend));
	}

	// Delete anything poking from the bottom
	editList.AddEdit(SDFEdit::CreateBox({ 0.0f, -3.05f, 0.f }, { 6.0f, 3.0f, 6.0f }, SDF_OP_SUBTRACTION));

	return editList;
}


SDFEditList Demos::IceCreamDemo(float deltaTime)
{
	SDFEditList editList(64, 8.0f);

	static UINT segments = 16;
	static float height = 0.1f;
	static float width = 1.0f;

	// Create cone
	for (UINT i = 0; i < segments; i++)
	{
		const float ir = width * static_cast<float>(i) / static_cast<float>(segments);
		const float y = 2.0f * height * static_cast<float>(i);

		Transform t{ 0.0f, y, 0.0f };

		editList.AddEdit(SDFEdit::CreateTorus(t, ir, height, SDF_OP_SMOOTH_UNION, 0.3f));
	}

	// Create ice cream
	editList.AddEdit(SDFEdit::CreateSphere({ 0.0f, 1.9f * segments * height, 0.0f }, 0.9f * width, SDF_OP_UNION, 0.0f));

	return editList;
}
