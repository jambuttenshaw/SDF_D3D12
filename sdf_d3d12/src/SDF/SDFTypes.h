#pragma once
#include "Framework/Transform.h"

using namespace DirectX;


#include "HlslCompat/ComputeHlslCompat.h"


union SDFShapeProperties
{
	struct {
		float Radius;
	} Sphere;

	struct {
		XMFLOAT3 Extents;
	} Box;

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

	struct
	{
	} Fractal;
};


struct SDFEdit
{
	// Transform of the primitive
	Transform PrimitiveTransform;

	// Properties of the primitive
	SDFShape Shape = SDF_SHAPE_SPHERE;

	// Shape-specific properties
	SDFShapeProperties ShapeProperties;

	SDFOperation Operation = SDF_OP_UNION;
	float BlendingRange = 0.0f;

	UINT MatTableIndex = 0;

	SDFEdit();

	bool DrawGui();

private:
	// Used by the GUI system
	void SetShapePropertiesToDefault();


public:

	// Constructor for each shape type
	static SDFEdit CreateSphere(const Transform& transform, float radius, SDFOperation op = SDF_OP_UNION, float blend = 0.0f, UINT matTableIndex = 0);
	static SDFEdit CreateBox(const Transform& transform, const XMFLOAT3& extents, SDFOperation op = SDF_OP_UNION, float blend = 0.0f, UINT matTableIndex = 0);
	static SDFEdit CreateTorus(const Transform& transform, float innerRadius, float torusRadius, SDFOperation op = SDF_OP_UNION, float blend = 0.0f, UINT matTableIndex = 0);
	static SDFEdit CreateOctahedron(const Transform& transform, float scale, SDFOperation op = SDF_OP_UNION, float blend = 0.0f, UINT matTableIndex = 0);
	static SDFEdit CreateBoxFrame(const Transform& transform, const XMFLOAT3& extents, float thickness, SDFOperation op = SDF_OP_UNION, float blend = 0.0f, UINT matTableIndex = 0);
	static SDFEdit CreateFractal(const Transform& transform, SDFOperation op = SDF_OP_UNION, float blend = 0.0f, UINT matTableIndex = 0);

private:
	static SDFEdit CreateGeneric(const Transform& transform, SDFOperation op, float blend, UINT matTableIndex = 0);

	static bool IsSmoothOperation(SDFOperation op);
};
