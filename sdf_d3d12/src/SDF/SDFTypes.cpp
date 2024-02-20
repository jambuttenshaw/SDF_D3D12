#include "pch.h"
#include "SDFTypes.h"

#include "imgui.h"
#include "Framework/Log.h"


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
		"Octahedron",
		"BoxFrame"
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
	case SDF_SHAPE_SPHERE:
		changed |= ImGui::DragFloat("Radius", &ShapeProperties.Sphere.Radius, 0.01f);
		break;
	case SDF_SHAPE_BOX:
		changed |= ImGui::DragFloat3("Extents", &ShapeProperties.Box.Extents.x, 0.01f);
		break;
	case SDF_SHAPE_TORUS:
		changed |= ImGui::DragFloat("InnerRadius", &ShapeProperties.Torus.InnerRadius, 0.01f);
		changed |= ImGui::DragFloat("TorusRadius", &ShapeProperties.Torus.TorusRadius, 0.01f);
		break;
	case SDF_SHAPE_OCTAHEDRON:
		changed |= ImGui::DragFloat("Scale", &ShapeProperties.Octahedron.Scale, 0.01f);
		break;
	case SDF_SHAPE_BOX_FRAME:
		changed |= ImGui::DragFloat3("Extents", &ShapeProperties.BoxFrame.Extents.x, 0.01f);
		changed |= ImGui::DragFloat("Thickness", &ShapeProperties.BoxFrame.Thickness, 0.01f);
		break;
	}

	int selectedOp = static_cast<int>(Operation);
	if (ImGui::Combo("Operation", &selectedOp, opNames, _countof(opNames)))
	{
		Operation = static_cast<SDFOperation>(selectedOp);
		changed = true;
	}

	changed |= ImGui::SliderFloat("Blending", &BlendingRange, 0.0f, 3.0f);

	return changed;
}


void SDFEdit::SetShapePropertiesToDefault()
{
	switch (Shape)
	{
	case SDF_SHAPE_SPHERE:
		ShapeProperties.Sphere.Radius = 1.0f;
		break;
	case SDF_SHAPE_BOX:
		ShapeProperties.Box.Extents = { 1.0f, 1.0f, 1.0f };
		break;
	case SDF_SHAPE_TORUS:
		ShapeProperties.Torus.InnerRadius = 1.0f;
		ShapeProperties.Torus.TorusRadius = 0.5f;
		break;
	case SDF_SHAPE_OCTAHEDRON:
		ShapeProperties.Octahedron.Scale = 1.0f;
		break;
	case SDF_SHAPE_BOX_FRAME:
		ShapeProperties.BoxFrame.Extents = { 1.0f, 1.0f, 1.0f };
		ShapeProperties.BoxFrame.Thickness= 0.1f;
		break;
	}
}



SDFEdit SDFEdit::CreateSphere(const Transform& transform, float radius, SDFOperation op, float blend)
{
	SDFEdit prim = CreateGeneric(transform, op, blend);
	prim.Shape = SDF_SHAPE_SPHERE;
	prim.ShapeProperties.Sphere.Radius = radius;
	return prim;
}

SDFEdit SDFEdit::CreateBox(const Transform& transform, const XMFLOAT3& extents, SDFOperation op, float blend)
{
	SDFEdit prim = CreateGeneric(transform, op, blend);
	prim.Shape = SDF_SHAPE_BOX;
	prim.ShapeProperties.Box.Extents = extents;
	return prim;
}

SDFEdit SDFEdit::CreateTorus(const Transform& transform, float innerRadius, float torusRadius, SDFOperation op, float blend)
{
	SDFEdit prim = CreateGeneric(transform, op, blend);
	prim.Shape = SDF_SHAPE_TORUS;
	prim.ShapeProperties.Torus.InnerRadius = innerRadius;
	prim.ShapeProperties.Torus.TorusRadius = torusRadius;
	return prim;
}

SDFEdit SDFEdit::CreateOctahedron(const Transform& transform, float scale, SDFOperation op, float blend)
{
	SDFEdit prim = CreateGeneric(transform, op, blend);
	prim.Shape = SDF_SHAPE_OCTAHEDRON;
	prim.ShapeProperties.Octahedron.Scale = scale;
	return prim;
}

SDFEdit SDFEdit::CreateBoxFrame(const Transform& transform, const XMFLOAT3& extents, float thickness, SDFOperation op, float blend)
{
	SDFEdit prim = CreateGeneric(transform, op, blend);
	prim.Shape = SDF_SHAPE_BOX_FRAME;
	prim.ShapeProperties.BoxFrame.Extents = extents;
	prim.ShapeProperties.BoxFrame.Thickness = thickness;
	return prim;
}

SDFEdit SDFEdit::CreateGeneric(const Transform& transform, SDFOperation op, float blend)
{
	SDFEdit prim;
	prim.PrimitiveTransform = transform;
	prim.Operation = op;
	prim.BlendingRange = blend;

	if ((prim.Operation & 2u) && prim.BlendingRange <= 0.0f)
	{
		LOG_WARN("Invalid blending range ({}) for smooth operation!", prim.BlendingRange);
	}

	return prim;
}
