#pragma once
#include "Framework/Transform.h"

using namespace DirectX;

// These MUST match the values defined in HLSL!
enum class SDFShape : UINT
{
	Sphere = 0,
	Box = 1,
	Plane = 2,
	Torus = 3,
	Octahedron = 4,
	BoxFrame = 5
};

enum class SDFOperation : UINT
{
	Union = 0,
	Subtraction,
	SmoothUnion,
	SmoothSubtraction
};

union SDFShapeProperties
{
	struct {
		float Radius;
	} Sphere;

	struct {
		XMFLOAT3 Extents;
	} Box;

	struct {
		XMFLOAT3 Normal;
		float Height;
	} Plane;

	struct {
		float InnerRadius;
		float TorusRadius;
	} Torus;

	struct {
		float Scale;
	} Octahedron;

	struct {
		XMFLOAT3 Extents;
		float Thickness;
	} BoxFrame;
};


struct SDFEdit
{
	// Transform of the primitive
	Transform PrimitiveTransform;

	// Properties of the primitive
	SDFShape Shape = SDFShape::Sphere;

	// Shape-specific properties
	SDFShapeProperties ShapeProperties;

	SDFOperation Operation = SDFOperation::Union;
	float BlendingFactor = 0.0f;

	// Color
	XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

	SDFEdit();

	bool DrawGui();

private:
	// Used by the GUI system
	void SetShapePropertiesToDefault();

public:

	// Constructor for each shape type
	static SDFEdit CreateSphere(const Transform& transform, float radius, SDFOperation op = SDFOperation::Union, float blend = 0.0f, const XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
	static SDFEdit CreateBox(const Transform& transform, const XMFLOAT3& extents, SDFOperation op = SDFOperation::Union, float blend = 0.0f, const XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
	static SDFEdit CreatePlane(const Transform& transform, const XMFLOAT3& normal, float height, SDFOperation op = SDFOperation::Union, float blend = 0.0f, const XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
	static SDFEdit CreateTorus(const Transform& transform, float innerRadius, float torusRadius, SDFOperation op = SDFOperation::Union, float blend = 0.0f, const XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
	static SDFEdit CreateOctahedron(const Transform& transform, float scale, SDFOperation op = SDFOperation::Union, float blend = 0.0f, const XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
	static SDFEdit CreateBoxFrame(const Transform& transform, const XMFLOAT3& extents, float thickness, SDFOperation op = SDFOperation::Union, float blend = 0.0f, const XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

private:
	static SDFEdit CreateGeneric(const Transform& transform, SDFOperation op, float blend, const XMFLOAT4& color);

};
