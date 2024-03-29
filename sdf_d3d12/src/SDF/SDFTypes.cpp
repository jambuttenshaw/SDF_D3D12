#include "pch.h"
#include "SDFTypes.h"

#include "imgui.h"
#include "Framework/GuiHelpers.h"
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
		"Torus",
		"Octahedron",
		"BoxFrame",
		"Fractal"
	};
	static const char* opNames[] =
	{
		"Union",
		"Subtraction",
		"SmoothUnion",
		"SmoothSubtraction"
	};

	bool changed = false;

	int selectedShape = static_cast<int>(Shape);
	if (ImGui::Combo("Shape", &selectedShape, shapeNames, ARRAYSIZE(shapeNames)))
	{
		Shape = static_cast<SDFShape>(selectedShape);
		// To avoid UB from reading wrong property of the union
		SetShapePropertiesToDefault();
		changed = true;
	}
	ImGui::Separator();

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
	case SDF_SHAPE_FRACTAL:
		break;
	}
	ImGui::Separator();

	int selectedOp = static_cast<int>(Operation);
	if (ImGui::Combo("Operation", &selectedOp, opNames, _countof(opNames)))
	{
		Operation = static_cast<SDFOperation>(selectedOp);
		changed = true;
	}

	{
		GuiHelpers::DisableScope disable(!IsSmoothOperation(Operation));

		changed |= ImGui::SliderFloat("Blending", &BlendingRange, 0.0f, 1.0f);
	}
	ImGui::Separator();

	{
		GuiHelpers::DisableScope disable(IsSubtractionOperation(Operation));

		int material = static_cast<int>(MatTableIndex);
		if (ImGui::SliderInt("Material", &material, 0, 3))
		{
			MatTableIndex = static_cast<UINT>(material);
			changed = true;
		}
	}

	return changed;
}

void SDFEdit::Validate()
{
	if (IsSmoothOperation(Operation) && BlendingRange <= 0.0f)
	{
		Operation = static_cast<SDFOperation>(Operation & ~2u);
	}
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
	case SDF_SHAPE_FRACTAL:
		break;
	}
}

bool SDFEdit::IsSubtractionOperation(SDFOperation op)
{
	return op & 1u;
}
bool SDFEdit::IsSmoothOperation(SDFOperation op)
{
	return op & 2u;
}




SDFEdit SDFEdit::CreateSphere(const Transform& transform, float radius, SDFOperation op, float blend, UINT matTableIndex)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, matTableIndex);
	prim.Shape = SDF_SHAPE_SPHERE;
	prim.ShapeProperties.Sphere.Radius = radius;
	return prim;
}

SDFEdit SDFEdit::CreateBox(const Transform& transform, const XMFLOAT3& extents, SDFOperation op, float blend, UINT matTableIndex)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, matTableIndex);
	prim.Shape = SDF_SHAPE_BOX;
	prim.ShapeProperties.Box.Extents = extents;
	return prim;
}

SDFEdit SDFEdit::CreateTorus(const Transform& transform, float innerRadius, float torusRadius, SDFOperation op, float blend, UINT matTableIndex)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, matTableIndex);
	prim.Shape = SDF_SHAPE_TORUS;
	prim.ShapeProperties.Torus.InnerRadius = innerRadius;
	prim.ShapeProperties.Torus.TorusRadius = torusRadius;
	return prim;
}

SDFEdit SDFEdit::CreateOctahedron(const Transform& transform, float scale, SDFOperation op, float blend, UINT matTableIndex)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, matTableIndex);
	prim.Shape = SDF_SHAPE_OCTAHEDRON;
	prim.ShapeProperties.Octahedron.Scale = scale;
	return prim;
}

SDFEdit SDFEdit::CreateBoxFrame(const Transform& transform, const XMFLOAT3& extents, float thickness, SDFOperation op, float blend, UINT matTableIndex)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, matTableIndex);
	prim.Shape = SDF_SHAPE_BOX_FRAME;
	prim.ShapeProperties.BoxFrame.Extents = extents;
	prim.ShapeProperties.BoxFrame.Thickness = thickness;
	return prim;
}

SDFEdit SDFEdit::CreateFractal(const Transform& transform, SDFOperation op, float blend, UINT matTableIndex)
{
	SDFEdit prim = CreateGeneric(transform, op, blend, matTableIndex);
	prim.Shape = SDF_SHAPE_FRACTAL;
	return prim;
}


SDFEdit SDFEdit::CreateGeneric(const Transform& transform, SDFOperation op, float blend, UINT matTableIndex)
{
	SDFEdit prim;
	prim.PrimitiveTransform = transform;
	prim.Operation = op;
	prim.BlendingRange = blend;
	prim.MatTableIndex = matTableIndex;

	if (IsSmoothOperation(prim.Operation) && prim.BlendingRange <= 0.0f)
	{
		prim.Operation = static_cast<SDFOperation>(prim.Operation & ~2u);
	}

	return prim;
}
