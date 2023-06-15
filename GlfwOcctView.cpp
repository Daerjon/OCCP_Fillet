#include "GlfwOcctView.h"

#include <opencascade/AIS_Shape.hxx>
#include <opencascade/Aspect_Handle.hxx>
#include <opencascade/Aspect_DisplayConnection.hxx>
#include <opencascade/BRepPrimAPI_MakeSphere.hxx>
#include <opencascade/Message.hxx>
#include <opencascade/Message_Messenger.hxx>
#include <opencascade/OpenGl_GraphicDriver.hxx>
#include <opencascade/TopAbs_ShapeEnum.hxx>
#include <opencascade/BRepBuilderAPI.hxx>
#include <opencascade/BRepTools.hxx>
#include <opencascade/BRepFeat_SplitShape.hxx>
#include <opencascade/BRepFeat.hxx>
#include <opencascade/TopTools_IndexedMapOfShape.hxx>
#include <opencascade/TopoDS.hxx>
#include <opencascade/TopExp.hxx>
#include <opencascade/BRepAdaptor_Surface.hxx>
#include <opencascade/BRepAdaptor_Curve.hxx>
#include <opencascade/Geom_SphericalSurface.hxx>
#include <opencascade/GeomAdaptor_Surface.hxx>
#include <opencascade/TopLoc_Location.hxx>
#include <opencascade/ShapeAnalysis_Surface.hxx>

#include <iostream>

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

std::string text = "";
bool checkFlatSurfaces = true;
bool checkBSplineSurfaces = false;
bool checkBezierSurfaces = false;
bool checkOffset = false;
bool checkCone = false;
bool checkSphere = false;
bool checkTorus = false;
bool checkAdjacentFaces = true;
bool checkNormals = true;
bool checkEdgesNumber = false;
bool checkWrapping = true;

namespace
{
  //! Convert GLFW mouse button into Aspect_VKeyMouse.
  static Aspect_VKeyMouse mouseButtonFromGlfw (int theButton)
  {
    switch (theButton)
    {
      case GLFW_MOUSE_BUTTON_LEFT:   return Aspect_VKeyMouse_LeftButton;
      case GLFW_MOUSE_BUTTON_RIGHT:  return Aspect_VKeyMouse_RightButton;
      case GLFW_MOUSE_BUTTON_MIDDLE: return Aspect_VKeyMouse_MiddleButton;
    }
    return Aspect_VKeyMouse_NONE;
  }

  //! Convert GLFW key modifiers into Aspect_VKeyFlags.
  static Aspect_VKeyFlags keyFlagsFromGlfw (int theFlags)
  {
    Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
    if ((theFlags & GLFW_MOD_SHIFT) != 0)
    {
      aFlags |= Aspect_VKeyFlags_SHIFT;
    }
    if ((theFlags & GLFW_MOD_CONTROL) != 0)
    {
      aFlags |= Aspect_VKeyFlags_CTRL;
    }
    if ((theFlags & GLFW_MOD_ALT) != 0)
    {
      aFlags |= Aspect_VKeyFlags_ALT;
    }
    if ((theFlags & GLFW_MOD_SUPER) != 0)
    {
      aFlags |= Aspect_VKeyFlags_META;
    }
    return aFlags;
  }
}

GlfwOcctView::GlfwOcctView()
{
}
GlfwOcctView::~GlfwOcctView()
{
}
GlfwOcctView* GlfwOcctView::toView (GLFWwindow* theWin)
{
  return static_cast<GlfwOcctView*>(glfwGetWindowUserPointer (theWin));
}
void GlfwOcctView::errorCallback (int theError, const char* theDescription)
{
  Message::DefaultMessenger()->Send (TCollection_AsciiString ("Error") + theError + ": " + theDescription, Message_Fail);
}
void GlfwOcctView::run()
{
  initWindow (800, 600, "Explode demo");
  initViewer();
  initDemoScene();
  if (myView.IsNull())
  {
    return;
  }

  myView->MustBeResized();
  myOcctWindow->Map();
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImGui_ImplGlfw_InitForOpenGL(myOcctWindow->getGlfwWindow(), true);
  ImGui_ImplOpenGL3_Init("#version 330");
  mainloop();
  cleanup();
}
void GlfwOcctView::initWindow (int theWidth, int theHeight, const char* theTitle)
{
  glfwSetErrorCallback (GlfwOcctView::errorCallback);
  glfwInit();
  const bool toAskCoreProfile = true;
  if (toAskCoreProfile)
  {
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
#if defined (__APPLE__)
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  }
  myOcctWindow = new GlfwOcctWindow (theWidth, theHeight, theTitle);
  glfwSetWindowUserPointer       (myOcctWindow->getGlfwWindow(), this);
  // window callback
  glfwSetWindowSizeCallback      (myOcctWindow->getGlfwWindow(), GlfwOcctView::onResizeCallback);
  glfwSetFramebufferSizeCallback (myOcctWindow->getGlfwWindow(), GlfwOcctView::onFBResizeCallback);
  // mouse callback
  glfwSetScrollCallback          (myOcctWindow->getGlfwWindow(), GlfwOcctView::onMouseScrollCallback);
  glfwSetMouseButtonCallback     (myOcctWindow->getGlfwWindow(), GlfwOcctView::onMouseButtonCallback);
  glfwSetCursorPosCallback       (myOcctWindow->getGlfwWindow(), GlfwOcctView::onMouseMoveCallback);
}
void GlfwOcctView::initViewer()
{
  if (myOcctWindow.IsNull()
   || myOcctWindow->getGlfwWindow() == nullptr)
  {
    return;
  }

  Handle(OpenGl_GraphicDriver) aGraphicDriver = new OpenGl_GraphicDriver (myOcctWindow->GetDisplay(), false);
  aGraphicDriver->SetBuffersNoSwap(true);
  Handle(V3d_Viewer) aViewer = new V3d_Viewer (aGraphicDriver);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  aViewer->SetDefaultTypeOfView (V3d_PERSPECTIVE);
  aViewer->ActivateGrid (Aspect_GT_Rectangular, Aspect_GDM_Lines);
  myView = aViewer->CreateView();
  myView->SetImmediateUpdate (false);
  myView->SetWindow (myOcctWindow, myOcctWindow->NativeGlContext());
  myContext = new AIS_InteractiveContext (aViewer);
}

void GlfwOcctView::LoadFile()
{
    //Clear Screen
    myContext->RemoveAll(false);

    //colors
    const Quantity_Color filletColor = Quantity_Color::Quantity_Color(Quantity_NOC_ALICEBLUE);
    const Quantity_Color planeColor = Quantity_Color::Quantity_Color(Quantity_NOC_BLUE1);
    const Quantity_Color bSplineColor = Quantity_Color::Quantity_Color(Quantity_NOC_LEMONCHIFFON3);
    const Quantity_Color bezierColor = Quantity_Color::Quantity_Color(Quantity_NOC_GOLD4);
    const Quantity_Color wrappingColor = Quantity_Color::Quantity_Color(Quantity_NOC_DEEPPINK4);
    const Quantity_Color lotsOfSidesColor = Quantity_Color::Quantity_Color(Quantity_NOC_BROWN);
    const Quantity_Color edgesColor = Quantity_Color::Quantity_Color(Quantity_NOC_ORANGE2);
    const Quantity_Color offsetColor = Quantity_Color::Quantity_Color(Quantity_NOC_KHAKI4);
    const Quantity_Color coneColor = Quantity_Color::Quantity_Color(Quantity_NOC_DARKTURQUOISE);
    const Quantity_Color sphereColor = Quantity_Color::Quantity_Color(Quantity_NOC_IVORY);
    const Quantity_Color torusColor = Quantity_Color::Quantity_Color(Quantity_NOC_BLANCHEDALMOND);
    const Quantity_Color nonCorrectNormalsColor = Quantity_Color::Quantity_Color(Quantity_NOC_GREEN);

    //constants
    constexpr float eps = 0.0001f;

    //Load model
    TopoDS_Shape s;
    BRep_Builder b;
    BRepTools::Read(s, text.c_str(), b);

    //Split faces
    TopTools_IndexedMapOfShape faces;
    TopTools_IndexedDataMapOfShapeListOfShape edge2face;
    TopExp::MapShapes(s, TopAbs_FACE, faces);
    TopExp::MapShapesAndAncestors(s, TopAbs_EDGE, TopAbs_FACE, edge2face);

    for (int i = 1; i <= faces.Extent(); i++)
    {
        TopoDS_Face topoFace = TopoDS::Face(faces(i));
        BRepAdaptor_Surface surf(topoFace, true);
        AIS_Shape* drawFace = new AIS_Shape(topoFace);
        //Discard flat planes
        if(checkFlatSurfaces && surf.GetType() == GeomAbs_Plane)
        {
            drawFace->SetColor(planeColor);
            myContext->Display(drawFace, AIS_Shaded, 0, false);
            continue;
        }
        //Discard BSpline Surfaces
        if (checkBSplineSurfaces && surf.GetType() == GeomAbs_BSplineSurface)
        {
            drawFace->SetColor(bSplineColor);
            myContext->Display(drawFace, AIS_Shaded, 0, false);
            continue;
        }
        //Discard Bezier Surfaces
        if (checkBezierSurfaces && surf.GetType() == GeomAbs_BezierSurface)
        {
            drawFace->SetColor(bezierColor);
            myContext->Display(drawFace, AIS_Shaded, 0, false);
            continue;
        }
        if (checkOffset && surf.GetType() == GeomAbs_OffsetSurface)
        {
            drawFace->SetColor(offsetColor);
            myContext->Display(drawFace, AIS_Shaded, 0, false);
            continue;
        }
        if (checkCone && surf.GetType() == GeomAbs_Cone)
        {
            drawFace->SetColor(coneColor);
            myContext->Display(drawFace, AIS_Shaded, 0, false);
            continue;
        }
        if (checkSphere && surf.GetType() == GeomAbs_Sphere)
        {
            drawFace->SetColor(sphereColor);
            myContext->Display(drawFace, AIS_Shaded, 0, false);
            continue;
        }
        if (checkTorus && surf.GetType() == GeomAbs_Torus)
        {
            drawFace->SetColor(torusColor);
            myContext->Display(drawFace, AIS_Shaded, 0, false);
            continue;
        }
        //Discard full cylinders and toruses
        if (checkWrapping)
        {
            gp_Vec FFu, FFv, LFu, LFv, FLu, FLv;
            gp_Pnt FF, LF, FL;
            surf.D1(surf.FirstUParameter(), surf.FirstVParameter(), FF, FFu, FFv);
            surf.D1(surf.LastUParameter(), surf.FirstVParameter(), LF, LFu, LFv);
            surf.D1(surf.FirstUParameter(), surf.LastVParameter(), FL, FLu, FLv);
            if ((FF.Distance(LF) < eps &&
                FFu.Crossed(FFv).CrossMagnitude(FLu.Crossed(FLv)) < eps) ||
                (FF.Distance(FL) < eps &&
                    FFu.Crossed(FFv).CrossMagnitude(LFu.Crossed(LFv)) < eps))
            {
                drawFace->SetColor(wrappingColor);
                myContext->Display(drawFace, AIS_Shaded, 0, false);
                continue;
            }
        }
        //Discard if lots of edges
        if (checkEdgesNumber)
        {
            TopTools_IndexedMapOfShape edges;
            TopExp::MapShapes(topoFace, TopAbs_EDGE, edges);
            if (edges.Extent() > 4)
            {
                drawFace->SetColor(edgesColor);
                myContext->Display(drawFace, AIS_Shaded, 0, false);
                continue;
            }
        }
        //Discard if more than 4 adjacent faces
        if (checkAdjacentFaces)
        {
            TopTools_IndexedMapOfShape edges;
            TopExp::MapShapes(topoFace, TopAbs_EDGE, edges);
            int adjnum = 0;
            TopoDS_Shape adj[5];
            for (int edge = 1; edge <= edges.Extent() && adjnum < 5; edge++)
                for (auto& fac : edge2face.FindFromKey(edges(edge)))
                    if (!fac.IsSame(topoFace))
                    {
                        bool flag = true;
                        for (int check = 0; flag && check < adjnum; check++)
                            if (fac.IsSame(adj[check]))
                                flag = false;
                        if (flag)
                        {
                            adj[adjnum] = fac;
                            adjnum++;
                        }
                    }
            if (adjnum > 4)
            {
                drawFace->SetColor(lotsOfSidesColor);
                myContext->Display(drawFace, AIS_Shaded, 0, false);
                continue;
            }
        }
        //Check normals
        if (checkNormals)
        {
            TopTools_IndexedMapOfShape edges;
            TopExp::MapShapes(topoFace, TopAbs_EDGE, edges);
            int adjnum = 0;
            int fuckup = 0;
            for (int edge = 1; edge <= edges.Extent(); edge++)
            {
                bool yup = false;
                TopTools_ListOfShape fac = edge2face.FindFromKey(edges(edge));
                if (fac.Extent() < 2)
                    continue;
                TopTools_IndexedMapOfShape vertices;
                TopExp::MapShapes(edges(edge), TopAbs_VERTEX, vertices);
                gp_Vec* normalsF1 = new gp_Vec[vertices.Extent()];
                gp_Pnt* positionF1 = new gp_Pnt[vertices.Extent()];
                gp_Vec* normalsF2 = new gp_Vec[vertices.Extent()];
                gp_Pnt* positionF2 = new gp_Pnt[vertices.Extent()];
                for (int ver = 1; ver <= vertices.Extent(); ver++)
                {
                    gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(vertices(ver)));
                    gp_Pnt2d pf1 = FaceParameters(TopoDS::Face(fac.First()), p);
                    gp_Pnt2d pf2 = FaceParameters(TopoDS::Face(fac.Last()), p);
                    gp_Vec f1u, f1v, f2u, f2v;
                    gp_Pnt f1, f2;
                    surf.D1(pf1.X(), pf1.Y(), f1, f1u, f1v);
                    surf.D1(pf2.X(), pf2.Y(), f2, f2u, f2v);
                    normalsF1[ver - 1] = f1u.Crossed(f1v);
                    positionF1[ver - 1] = f1;
                    normalsF2[ver - 1] = f2u.Crossed(f2v);
                    positionF2[ver - 1] = f2;
                }
                bool range = false;
                for (int help = 0; help < vertices.Extent(); help++)
                {
                    for (int help2 = 0; help2 < vertices.Extent(); help2++)
                    {
                        if (positionF1[help].Distance(positionF2[help2]) < eps)
                        {
                            if (normalsF1[help].CrossMagnitude(normalsF2[help2]) < eps)
                            {
                               yup = true;
                            }
                        }
                        else
                        {
                            range = true;
                        }
                    }
                }
                if (yup || range)
                {
                    adjnum++;
                }
            }
            if (adjnum < 1)
            {
                drawFace->SetColor(nonCorrectNormalsColor);
                myContext->Display(drawFace, AIS_Shaded, 0, false);
                continue;
            }
        }
        //Is possibly as fillet
        drawFace->SetColor(filletColor);
        myContext->Display(drawFace, AIS_Shaded, 0, false);
    }
}

gp_Pnt2d GlfwOcctView::FaceParameters(TopoDS_Face& face, gp_Pnt& pt)
{
    const Handle(Geom_Surface)& surface = BRep_Tool::Surface(face);
    ShapeAnalysis_Surface sas(surface);
    gp_Pnt2d uv = sas.ValueOfUV(pt, 0.00000001);
    return uv;
}

void GlfwOcctView::initDemoScene()
{
  if (myContext.IsNull())
  {
    return;
  }

  gp_Ax2 anAxis;
  LoadFile();
}

void GlfwOcctView::mainloop()
{
  while (!glfwWindowShouldClose (myOcctWindow->getGlfwWindow()))
  {
    // glfwPollEvents() for continuous rendering (immediate return if there are no new events)
    // and glfwWaitEvents() for rendering on demand (something actually happened in the viewer)
    glfwPollEvents();
    //glfwWaitEvents();
    if (!myView.IsNull())
    {
      FlushViewEvents (myContext, myView, true);
      myView->Redraw();
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      ImGui::Begin("Debug");

      ImGui::InputText("File", &text);
      if (ImGui::Button("Load"))
      {
          LoadFile();
      }
      
      ImGui::Checkbox("Flats", &checkFlatSurfaces);
      ImGui::Checkbox("BSplines", &checkBSplineSurfaces);
      ImGui::Checkbox("Bezier", &checkBezierSurfaces);
      ImGui::Checkbox("Offset", &checkOffset);
      ImGui::Checkbox("Cone", &checkCone);
      ImGui::Checkbox("Sphere", &checkSphere);
      ImGui::Checkbox("Torus", &checkTorus);
      ImGui::Checkbox("Edges count", &checkEdgesNumber);
      ImGui::Checkbox("Adjacent Number", &checkAdjacentFaces);
      ImGui::Checkbox("Wrapping", &checkWrapping);
      ImGui::Checkbox("Normals", &checkNormals);

      ImGui::End();

      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      glfwSwapBuffers(myOcctWindow->getGlfwWindow());
    }
  }
}

void GlfwOcctView::cleanup()
{
  if (!myView.IsNull())
  {
    myView->Remove();
  }
  if (!myOcctWindow.IsNull())
  {
    myOcctWindow->Close();
  }
  glfwTerminate();
}

void GlfwOcctView::onResize (int theWidth, int theHeight)
{
  if (theWidth  != 0
   && theHeight != 0
   && !myView.IsNull())
  {
    myView->Window()->DoResize();
    myView->MustBeResized();
    myView->Invalidate();
    myView->Redraw();
  }
}

void GlfwOcctView::onMouseScroll (double theOffsetX, double theOffsetY)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;
  if (!myView.IsNull())
  {
    UpdateZoom (Aspect_ScrollDelta (myOcctWindow->CursorPosition(), int(theOffsetY * 8.0)));
  }
}

void GlfwOcctView::onMouseButton (int theButton, int theAction, int theMods)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;
  if (myView.IsNull()) { return; }

  const Graphic3d_Vec2i aPos = myOcctWindow->CursorPosition();
  if (theAction == GLFW_PRESS)
  {
    PressMouseButton (aPos, mouseButtonFromGlfw (theButton), keyFlagsFromGlfw (theMods), false);
  }
  else
  {
    ReleaseMouseButton (aPos, mouseButtonFromGlfw (theButton), keyFlagsFromGlfw (theMods), false);
  }
}

void GlfwOcctView::onMouseMove (int thePosX, int thePosY)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return;
  const Graphic3d_Vec2i aNewPos (thePosX, thePosY);
  if (!myView.IsNull())
  {
    UpdateMousePosition (aNewPos, PressedMouseButtons(), LastMouseFlags(), false);
  }
}
