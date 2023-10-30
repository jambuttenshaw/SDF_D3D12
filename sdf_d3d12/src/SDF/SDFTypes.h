#pragma once

using namespace DirectX;

// These MUST match the values defined in HLSL!
enum class SDFShape : UINT
{
	Sphere = 0,
	Box = 1,
	Plane = 2,
	Torus = 3,
	Octahedron = 4
};

enum class SDFOperation : UINT
{
	Union = 0,
	Subtraction = 1,
	Intersection = 2,
	SmoothUnion = 3,
	SmoothSubtraction = 4,
	SmoothIntersection = 5,
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
};


struct SDFPrimitive
{
	// Properties of the primitive

	SDFShape Shape = SDFShape::Sphere;

	// Shape-specific properties
	SDFShapeProperties ShapeProperties;

	SDFOperation Operation = SDFOperation::Union;
	float BlendingFactor = 0.0f;

	// Color
	XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

	SDFPrimitive();

	// Constructor for each shape type
	static SDFPrimitive CreateSphere(float radius, SDFOperation op, float blend, const XMFLOAT4& color);
	static SDFPrimitive CreateBox(const XMFLOAT3& extents, SDFOperation op, float blend, const XMFLOAT4& color);
	static SDFPrimitive CreatePlane(const XMFLOAT3 normal, float height, SDFOperation op, float blend, const XMFLOAT4& color);
	static SDFPrimitive CreateTorus(float innerRadius, float torusRadius, SDFOperation op, float blend, const XMFLOAT4& color);
	static SDFPrimitive CreateOctahedron(float scale, SDFOperation op, float blend, const XMFLOAT4& color);
};
