#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Tool.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <NCollection_Sequence.hxx>
#include <Poly_Triangulation.hxx>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ColorType.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <cstdio>
#include <sstream>
#include <string>

int main() {
  const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();

  int faces = 0;
  for (TopExp_Explorer it(box, TopAbs_FACE); it.More(); it.Next()) {
    ++faces;
  }
  if (faces != 6) {
    std::fprintf(stderr, "expected 6 faces, got %d\n", faces);
    return 1;
  }

  // Write a named, colored box to STEP in memory via XCAF.
  const occ::handle<TDocStd_Document> source = new TDocStd_Document("BinXCAF");
  occ::handle<XCAFDoc_ShapeTool> source_shapes =
      XCAFDoc_DocumentTool::ShapeTool(source->Main());
  occ::handle<XCAFDoc_ColorTool> source_colors =
      XCAFDoc_DocumentTool::ColorTool(source->Main());
  const TDF_Label box_label = source_shapes->AddShape(box);
  TDataStd_Name::Set(box_label, "red_box");
  source_colors->SetColor(box_label, Quantity_Color(Quantity_NOC_RED),
                          XCAFDoc_ColorSurf);

  STEPCAFControl_Writer writer;
  if (!writer.Transfer(source)) {
    std::fprintf(stderr, "STEPCAFControl_Writer::Transfer failed\n");
    return 1;
  }
  std::ostringstream step_out;
  if (writer.WriteStream(step_out) != IFSelect_RetDone) {
    std::fprintf(stderr, "STEPCAFControl_Writer::WriteStream failed\n");
    return 1;
  }
  const std::string step = step_out.str();
  if (step.find("ISO-10303-21") == std::string::npos) {
    std::fprintf(stderr, "expected STEP header ISO-10303-21\n");
    return 1;
  }

  // Read the STEP stream back into an XCAF document.
  STEPCAFControl_Reader reader;
  reader.SetNameMode(true);
  reader.SetColorMode(true);
  std::istringstream step_in(step);
  if (reader.ReadStream("box.step", step_in) != IFSelect_RetDone) {
    std::fprintf(stderr, "STEPCAFControl_Reader::ReadStream failed\n");
    return 1;
  }
  const occ::handle<TDocStd_Document> doc = new TDocStd_Document("BinXCAF");
  if (!reader.Transfer(doc)) {
    std::fprintf(stderr, "STEPCAFControl_Reader::Transfer failed\n");
    return 1;
  }

  occ::handle<XCAFDoc_ShapeTool> shapes =
      XCAFDoc_DocumentTool::ShapeTool(doc->Main());
  occ::handle<XCAFDoc_ColorTool> colors =
      XCAFDoc_DocumentTool::ColorTool(doc->Main());
  NCollection_Sequence<TDF_Label> free_shapes;
  shapes->GetFreeShapes(free_shapes);
  if (free_shapes.Length() != 1) {
    std::fprintf(stderr, "expected 1 free shape, got %d\n",
                 free_shapes.Length());
    return 1;
  }
  const TDF_Label label = free_shapes.Value(1);

  occ::handle<TDataStd_Name> name;
  if (!label.FindAttribute(TDataStd_Name::GetID(), name) ||
      TCollection_AsciiString(name->Get()) != "red_box") {
    std::fprintf(stderr, "expected shape name 'red_box'\n");
    return 1;
  }

  Quantity_Color color;
  if (!colors->GetColor(label, XCAFDoc_ColorSurf, color) ||
      !color.IsEqual(Quantity_Color(Quantity_NOC_RED))) {
    std::fprintf(stderr, "expected red surface color\n");
    return 1;
  }

  const TopoDS_Shape imported_shape = XCAFDoc_ShapeTool::GetShape(label);
  BRepMesh_IncrementalMesh mesher(imported_shape, 0.1, false, 0.5, false);
  if (!mesher.IsDone()) {
    std::fprintf(stderr, "BRepMesh_IncrementalMesh failed\n");
    return 1;
  }

  int triangles = 0;
  for (TopExp_Explorer it(imported_shape, TopAbs_FACE); it.More(); it.Next()) {
    TopLoc_Location location;
    const occ::handle<Poly_Triangulation> triangulation =
        BRep_Tool::Triangulation(TopoDS::Face(it.Current()), location);
    if (!triangulation.IsNull()) {
      triangles += triangulation->NbTriangles();
    }
  }
  if (triangles != 12) {
    std::fprintf(stderr, "expected 12 triangles, got %d\n", triangles);
    return 1;
  }

  return 0;
}
