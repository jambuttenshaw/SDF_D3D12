#include "pch.h"
#include "SDFTypes.h"


SDFPrimitive::SDFPrimitive()
{
	ShapeProperties.Sphere.Radius = 1.0f;
}

SDFPrimitive SDFPrimitive::CreateSphere(float radius, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFPrimitive prim;
	prim.Shape = SDFShape::Sphere;
	prim.ShapeProperties.Sphere.Radius = radius;
	prim.Operation = op;
	prim.BlendingFactor = blend;
	prim.Color = color;
	return prim;
}

SDFPrimitive SDFPrimitive::CreateBox(const XMFLOAT3& extents, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFPrimitive prim;
	prim.Shape = SDFShape::Box;
	prim.ShapeProperties.Box.Extents = extents;
	prim.Operation = op;
	prim.BlendingFactor = blend;
	prim.Color = color;
	return prim;
}

SDFPrimitive SDFPrimitive::CreatePlane(const XMFLOAT3 normal, float height, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFPrimitive prim;
	prim.Shape = SDFShape::Plane;
	prim.ShapeProperties.Plane.Normal = normal;
	prim.ShapeProperties.Plane.Height = height;
	prim.Operation = op;
	prim.BlendingFactor = blend;
	prim.Color = color;
	return prim;
}

SDFPrimitive SDFPrimitive::CreateTorus(float innerRadius, float torusRadius, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFPrimitive prim;
	prim.Shape = SDFShape::Torus;
	prim.ShapeProperties.Torus.InnerRadius = innerRadius;
	prim.ShapeProperties.Torus.TorusRadius = torusRadius;
	prim.Operation = op;
	prim.BlendingFactor = blend;
	prim.Color = color;
	return prim;
}

SDFPrimitive SDFPrimitive::CreateOctahedron(float scale, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFPrimitive prim;
	prim.Shape = SDFShape::Octahedron;
	prim.ShapeProperties.Octahedron.Scale = scale;
	prim.Operation = op;
	prim.BlendingFactor = blend;
	prim.Color = color;
	return prim;
}
