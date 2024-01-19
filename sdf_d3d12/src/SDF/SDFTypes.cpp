#include "pch.h"
#include "SDFTypes.h"

#include "imgui.h"


SDFEdit::SDFEdit()
{
	ShapeProperties.Sphere.Radius = 1.0f;
}


bool SDFEdit::DrawGui()
{
	static const char* shapeNames[] = 
	{
		"Sphere",
		"Box",
		"Plane",
		"Torus",
		"Octahedron"
	};
	static const char* opNames[] =
	{
		"Union",
		"Subtraction",
		"Intersection",
		"SmoothUnion",
		"SmoothSubtraction",
		"SmoothIntersection"
	};

	bool changed = false;

	int selectedShape = static_cast<int>(Shape);
	if (ImGui::Combo("Shape", &selectedShape, shapeNames, _countof(shapeNames)))
	{
		Shape = static_cast<SDFShape>(selectedShape);
		// To avoid UB from reading wrong property of the union
		SetShapePropertiesToDefault();
		changed = true;
	}

	switch (Shape)
	{
	case SDFShape::Sphere:
		changed |= ImGui::DragFloat("Radius", &ShapeProperties.Sphere.Radius, 0.01f);
		break;
	case SDFShape::Box:
		changed |= ImGui::DragFloat3("Extents", &ShapeProperties.Box.Extents.x, 0.01f);
		break;
	case SDFShape::Plane:
		changed |= ImGui::DragFloat3("Normal", &ShapeProperties.Plane.Normal.x, 0.01f);
		changed |= ImGui::DragFloat("Height", &ShapeProperties.Plane.Height, 0.01f);
		break;
	case SDFShape::Torus:
		changed |= ImGui::DragFloat("InnerRadius", &ShapeProperties.Torus.InnerRadius, 0.01f);
		changed |= ImGui::DragFloat("TorusRadius", &ShapeProperties.Torus.TorusRadius, 0.01f);
		break;
	case SDFShape::Octahedron:
		changed |= ImGui::DragFloat("Scale", &ShapeProperties.Octahedron.Scale, 0.01f);
		break;
	}

	int selectedOp = static_cast<int>(Operation);
	if (ImGui::Combo("Operation", &selectedOp, opNames, _countof(opNames)))
	{
		Operation = static_cast<SDFOperation>(selectedOp);
		changed = true;
	}

	changed |= ImGui::SliderFloat("Blending", &BlendingFactor, 0.0f, 3.0f);
	changed |= ImGui::ColorEdit3("Color", &Color.x);

	return changed;
}


void SDFEdit::SetShapePropertiesToDefault()
{
	switch (Shape)
	{
	case SDFShape::Sphere:
		ShapeProperties.Sphere.Radius = 1.0f;
		break;
	case SDFShape::Box:
		ShapeProperties.Box.Extents = { 1.0f, 1.0f, 1.0f };
		break;
	case SDFShape::Plane:
		ShapeProperties.Plane.Normal = { 0.0f, 1.0f, 0.0f };
		ShapeProperties.Plane.Height = 1.0f;
		break;
	case SDFShape::Torus:
		ShapeProperties.Torus.InnerRadius = 1.0f;
		ShapeProperties.Torus.TorusRadius = 0.5f;
		break;
	case SDFShape::Octahedron:
		ShapeProperties.Octahedron.Scale = 1.0f;
		break;
	}
}



SDFEdit SDFEdit::CreateSphere(const Transform& transform, float radius, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, color);
	prim.Shape = SDFShape::Sphere;
	prim.ShapeProperties.Sphere.Radius = radius;
	return prim;
}

SDFEdit SDFEdit::CreateBox(const Transform& transform, const XMFLOAT3& extents, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, color);
	prim.Shape = SDFShape::Box;
	prim.ShapeProperties.Box.Extents = extents;
	return prim;
}

SDFEdit SDFEdit::CreatePlane(const Transform& transform, const XMFLOAT3& normal, float height, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, color);
	prim.Shape = SDFShape::Plane;
	prim.ShapeProperties.Plane.Normal = normal;
	prim.ShapeProperties.Plane.Height = height;
	return prim;
}

SDFEdit SDFEdit::CreateTorus(const Transform& transform, float innerRadius, float torusRadius, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, color);
	prim.Shape = SDFShape::Torus;
	prim.ShapeProperties.Torus.InnerRadius = innerRadius;
	prim.ShapeProperties.Torus.TorusRadius = torusRadius;
	return prim;
}

SDFEdit SDFEdit::CreateOctahedron(const Transform& transform, float scale, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, color);
	prim.Shape = SDFShape::Octahedron;
	prim.ShapeProperties.Octahedron.Scale = scale;
	return prim;
}

SDFEdit SDFEdit::CreateGeneric(const Transform& transform, SDFOperation op, float blend, const XMFLOAT4& color)
{
	SDFEdit prim;
	prim.PrimitiveTransform = transform;
	prim.Operation = op;
	prim.BlendingFactor = blend;
	prim.Color = color;
	return prim;
}



