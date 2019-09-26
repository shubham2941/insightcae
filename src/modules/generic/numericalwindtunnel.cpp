/*
 * This file is part of Insight CAE, a workbench for Computer-Aided Engineering
 * Copyright (C) 2014  Hannes Kroeger <hannes@kroegeronline.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "numericalwindtunnel.h"

#include "openfoam/blockmesh.h"
#include "openfoam/openfoamtools.h"
#include "openfoam/snappyhexmesh.h"
#include "openfoam/paraview.h"


// #include "boost/thread/mutex.hpp"

using namespace arma;
using namespace std;
using namespace boost;
using namespace boost::assign;
using namespace boost::filesystem;


namespace insight 
{


addToAnalysisFactoryTable(NumericalWindtunnel);

NumericalWindtunnel::NumericalWindtunnel(const ParameterSet& ps, const boost::filesystem::path& exepath)
: OpenFOAMAnalysis("Numerical Wind Tunnel", "", ps, exepath)
{

}


boost::mutex mtx;

void NumericalWindtunnel::calcDerivedInputData()
{
  
  Parameters p(parameters_);
  
  double bbdefl=0.5;

  gp_Trsf rot; rot.SetTransformation
  (
    gp_Ax3(gp_Pnt(0,0,0), gp_Dir(0,0,1), gp_Dir(-1,0,0)), // other way round => wrong
    gp_Ax3(gp_Pnt(0,0,0), toVec<gp_Dir>(p.geometry.upwarddir), toVec<gp_Dir>(p.geometry.forwarddir))
  );

  arma::mat bb; // bounding box in SI, rotated to wind tunnel CS

  std::string geom_file_ext = p.geometry.objectfile.filename().extension().string();
  boost::to_lower(geom_file_ext);

  if (geom_file_ext==".stl" || geom_file_ext==".stlb")
  {
    vtk_ChangeCS trsf(
          std::bind(&gp_Trsf::Value, &rot, std::placeholders::_1, std::placeholders::_2),
          3, 1
         );

    bb = p.geometryscale * STLBndBox(readSTL(p.geometry.objectfile, { &trsf }));
  }
  else
  {
    boost::mutex::scoped_lock lock(mtx);

    auto obj = new cad::Transform(cad::Feature::CreateFromFile(p.geometry.objectfile.c_str()), rot);
    bb = p.geometryscale * obj->modelBndBox(bbdefl);
  }

  std::cout<<"bounding box minp = "<<bb.col(0)<<std::endl;
  std::cout<<"bounding box maxp = "<<bb.col(1)<<std::endl;

  arma::mat pmin=bb.col(0);
  arma::mat pmax=bb.col(1);

  gp_Trsf sc; sc.SetScaleFactor(p.geometryscale);
  gp_Trsf tr; tr.SetTranslation(gp_Vec(-pmin(0), -0.5*(pmax(1)+pmin(1)), -pmin(2)));

  cad_to_cfd_ = tr.Multiplied(sc).Multiplied(rot);

  l_=(pmax(0)-pmin(0));
  w_=(pmax(1)-pmin(1));
  h_=(pmax(2)-pmin(2));

  Lref_ = arma::norm( bb.col(1)-bb.col(0), 2);
}




void NumericalWindtunnel::createMesh(insight::OpenFOAMCase& cm)
{
  path dir = executionPath();
  Parameters p(parameters_);
  
  boost::filesystem::path objectSTLFile = executionPath()/
   "constant"/"triSurface"/
   (p.geometry.objectfile.filename().stem().string()+".stlb");

  cm.insert(new MeshingNumerics(cm, MeshingNumerics::Parameters()
    .set_np(p.run.np)
  ));
  cm.createOnDisk(executionPath());
  

  double Lupstream = Lref_*p.geometry.LupstreamByL;
  double Ldownstream = Lref_*p.geometry.LdownstreamByL;
  double Lup = Lref_*p.geometry.LupByH;
  double Laside = Lref_*p.geometry.LasideByW;
  
  if (Lup<=0) throw insight::Exception("LupByL*L has to be larger than h!");
  if (Laside<=0) throw insight::Exception("LasideByL*L has to be larger than 0.5*w!");

  
  double dx=Lref_/double(p.mesh.nx);

  int nx=std::max(1, int(l_/dx));
  int ny=std::max(1, int(w_/dx));
  int nz=std::max(1, int(h_/dx));

  int n_upstream=bmd::GradingAnalyzer(p.mesh.grad_upstream).calc_n(dx, Lupstream);
  int n_downstream=bmd::GradingAnalyzer(p.mesh.grad_downstream).calc_n(dx, Ldownstream);
  int n_up=bmd::GradingAnalyzer(p.mesh.grad_up).calc_n(dx, Lup-h_);
  int n_aside=bmd::GradingAnalyzer(p.mesh.grad_aside).calc_n(dx, Laside-0.5*w_);
  

  using namespace insight::bmd;
  std::unique_ptr<blockMesh> bmd(new blockMesh(cm));
  bmd->setScaleFactor(1.0);
  bmd->setDefaultPatch("walls", "patch");


  Patch& inlet = 	bmd->addPatch("inlet", new Patch());
  Patch& outlet = 	bmd->addPatch("outlet", new Patch());
  Patch& side1 = 	bmd->addPatch("side1", new Patch());
  Patch& side2 = 	bmd->addPatch("side2", new Patch());
  Patch& top = 		bmd->addPatch("top", new Patch("symmetryPlane"));
  Patch& floor = 	bmd->addPatch("floor", new Patch("wall"));

  // points in cross section
  std::map<int, Point> pts = boost::assign::map_list_of       
	(0, 	vec3( -Lupstream, 0, 0))
	(1, 	vec3( 0, 0, 0))
        (2, 	vec3( l_, 0, 0))
        (3, 	vec3( l_+Ldownstream, 0, 0))
	(4, 	vec3( -Lupstream, 0, h_))
	(5, 	vec3( 0, 0, h_))
        (6, 	vec3( l_, 0, h_))
        (7, 	vec3( l_+Ldownstream, 0, h_))
	(8, 	vec3( -Lupstream, 0, Lup))
	(9, 	vec3( 0, 0, Lup))
        (10, 	vec3( l_, 0, Lup))
        (11, 	vec3( l_+Ldownstream, 0, Lup))
    ;
  arma::mat Lv=vec3(0,1,0);

  int nzs[]={n_aside, ny, n_aside};
  double grads[]={1./p.mesh.grad_aside, 1, p.mesh.grad_aside};
  arma::mat y0[]={vec3(0,Laside,0), vec3(0,0.5*w_,0), vec3(0,-0.5*w_,0), vec3(0,-Laside,0)};
  
  for (int i=0; i<3; i++)
  {
    {
      Block& bl = bmd->addBlock
      (
	new Block(P_8(
	    pts[0]+y0[i], pts[1]+y0[i], pts[5]+y0[i], pts[4]+y0[i],
	    pts[0]+y0[i+1], pts[1]+y0[i+1], pts[5]+y0[i+1], pts[4]+y0[i+1]
	  ),
	  n_upstream, nz, nzs[i],
	  list_of<Block::Grading> (1./p.mesh.grad_upstream) (1.) (grads[i])
	)
      );
      
      inlet.addFace(bl.face("0473"));
      floor.addFace(bl.face("0154"));
      if (i==0) side1.addFace(bl.face("0321"));
      if (i==2) side2.addFace(bl.face("4567"));
    }
    {
      Block& bl = bmd->addBlock
      (
	new Block(P_8(
	    pts[1]+y0[i], pts[2]+y0[i], pts[6]+y0[i], pts[5]+y0[i],
	    pts[1]+y0[i+1], pts[2]+y0[i+1], pts[6]+y0[i+1], pts[5]+y0[i+1]
	  ),
          nx, nz, nzs[i],
	  list_of<Block::Grading> (1.) (1.) (grads[i])
	)
      );
      floor.addFace(bl.face("0154"));
      if (i==0) side1.addFace(bl.face("0321"));
      if (i==2) side2.addFace(bl.face("4567"));
    }
    {
      Block& bl = bmd->addBlock
      (
	new Block(P_8(
	    pts[2]+y0[i], pts[3]+y0[i], pts[7]+y0[i], pts[6]+y0[i],
	    pts[2]+y0[i+1], pts[3]+y0[i+1], pts[7]+y0[i+1], pts[6]+y0[i+1]
	  ),
	  n_downstream, nz, nzs[i],
	  list_of<Block::Grading> (p.mesh.grad_downstream) (1.) (grads[i])
	)
      );
      outlet.addFace(bl.face("1265"));
      floor.addFace(bl.face("0154"));
      if (i==0) side1.addFace(bl.face("0321"));
      if (i==2) side2.addFace(bl.face("4567"));
    }
    {
      Block& bl = bmd->addBlock
      (
	new Block(P_8(
	    pts[4]+y0[i], pts[5]+y0[i], pts[9]+y0[i], pts[8]+y0[i],
	    pts[4]+y0[i+1], pts[5]+y0[i+1], pts[9]+y0[i+1], pts[8]+y0[i+1]
	  ),
	  n_upstream, n_up, nzs[i],
	  list_of<Block::Grading> (1./p.mesh.grad_upstream) (p.mesh.grad_up) (grads[i])
	)
      );
      inlet.addFace(bl.face("0473"));
      top.addFace(bl.face("2376"));
      if (i==0) side1.addFace(bl.face("0321"));
      if (i==2) side2.addFace(bl.face("4567"));
    }
    {
      Block& bl = bmd->addBlock
      (
	new Block(P_8(
	    pts[5]+y0[i], pts[6]+y0[i], pts[10]+y0[i], pts[9]+y0[i],
	    pts[5]+y0[i+1], pts[6]+y0[i+1], pts[10]+y0[i+1], pts[9]+y0[i+1]
	  ),
          nx, n_up, nzs[i],
	  list_of<Block::Grading> (1) (p.mesh.grad_up) (grads[i])
	)
      );
      top.addFace(bl.face("2376"));
      if (i==0) side1.addFace(bl.face("0321"));
      if (i==2) side2.addFace(bl.face("4567"));
    }
    {
      Block& bl = bmd->addBlock
      (
	new Block(P_8(
	    pts[6]+y0[i], pts[7]+y0[i], pts[11]+y0[i], pts[10]+y0[i],
	    pts[6]+y0[i+1], pts[7]+y0[i+1], pts[11]+y0[i+1], pts[10]+y0[i+1]
	  ),
	  n_downstream, n_up, nzs[i],
	  list_of<Block::Grading> (p.mesh.grad_downstream) (p.mesh.grad_up) (grads[i])
	)
      );
      outlet.addFace(bl.face("1265"));
      top.addFace(bl.face("2376"));
      if (i==0) side1.addFace(bl.face("0321"));
      if (i==2) side2.addFace(bl.face("4567"));
    }
  }
  
  cm.insert(bmd.release());
  
  cm.createOnDisk(executionPath());
  cm.executeCommand(executionPath(), "blockMesh");  
    
  create_directory(objectSTLFile.parent_path());

  std::string geom_file_ext = p.geometry.objectfile.filename().extension().string();
  boost::to_lower(geom_file_ext);

  if (geom_file_ext==".stl" || geom_file_ext==".stlb")
  {
    vtk_ChangeCS trsf(
          std::bind(&gp_Trsf::Value, &cad_to_cfd_, std::placeholders::_1, std::placeholders::_2),
          3, 1
         );
    writeSTL( readSTL(p.geometry.objectfile, { &trsf }), objectSTLFile );
  }
  else
  {
    boost::mutex::scoped_lock lock(mtx);

    auto obj = new cad::Transform(cad::Feature::CreateFromFile(p.geometry.objectfile.c_str()),
                                  cad_to_cfd_);
    obj->saveAs(objectSTLFile);
  }


  snappyHexMeshConfiguration::Parameters shm_cfg;
  
  shm_cfg.features.push_back(snappyHexMeshFeats::FeaturePtr(
      new snappyHexMeshFeats::Geometry(snappyHexMeshFeats::Geometry::Parameters()
    .set_name("object")
    .set_minLevel(p.mesh.lmsurf)
    .set_maxLevel(p.mesh.lxsurf)
    .set_nLayers(p.mesh.nlayer)
    .set_fileName(objectSTLFile)
  )));
  
  shm_cfg.features.push_back(snappyHexMeshFeats::FeaturePtr(new snappyHexMeshFeats::RefinementBox(snappyHexMeshFeats::RefinementBox::Parameters()
    .set_min(vec3(-0.33*l_, -(0.5+0.5)*w_, 0))
    .set_max(vec3(2.0*l_, (0.5+0.5)*w_, 1.33*h_))
    
    .set_name("refinement_box")
    .set_level(p.mesh.boxlevel)
  )));
  shm_cfg.features.push_back(snappyHexMeshFeats::FeaturePtr(new snappyHexMeshFeats::RefinementBox(snappyHexMeshFeats::RefinementBox::Parameters()
    .set_min(vec3(0.8*l_, -(0.5+0.25)*w_, 0))
    .set_max(vec3(1.5*l_, (0.5+0.25)*w_, 1.2*h_))
    
    .set_name("refinement_rear")
    .set_level(p.mesh.rearlevel)
  )));
  
  shm_cfg.PiM.push_back(vec3(-0.999*Lupstream,1e-6,1e-6));

  shm_cfg
  .set_tlayer ( p.mesh.tlayer )
  .set_erlayer ( 1.3 )
  ;
  
  snappyHexMesh
  (
    cm, executionPath(),
    shm_cfg
  );

  
  resetMeshToLatestTimestep(cm, executionPath(), true);
  
}




void NumericalWindtunnel::createCase(insight::OpenFOAMCase& cm)
{
  Parameters p(parameters_);

  //double vside=tan(yaw*M_PI/180.)*p.operation.v;

  double turbI=0.01; // Free-stream turbulence
  double turbL=0.001*h_; // Free-stream turbulence length scale => very low 0.1% of car height

  path dir = executionPath();

  OFDictData::dict boundaryDict;
  cm.parseBoundaryDict(dir, boundaryDict);

  cm.insert(new steadyIncompressibleNumerics(cm, steadyIncompressibleNumerics::Parameters()
    .set_writeInterval(100.0)
    .set_purgeWrite(0)
    .set_endTime(1000.0)
    .set_deltaT(1)
  ));
  cm.insert(new forces(cm, forces::Parameters()
    .set_rhoInf(p.fluid.rho)
    .set_patches(list_of<std::string>("\"(object.*)\""))
  ));
  cm.insert(new singlePhaseTransportProperties(cm, singlePhaseTransportProperties::Parameters()
    .set_nu(p.fluid.nu)
  ));

  cm.insert(new SimpleBC(cm, "top", boundaryDict, "symmetryPlane"));

  {
    cm.insert(new PressureOutletBC(cm, "side1", boundaryDict));
  }

  {
    cm.insert(new PressureOutletBC(cm, "side2", boundaryDict));
  }

  cm.insert(new WallBC(cm, "floor", boundaryDict, WallBC::Parameters()
    .set_wallVelocity(vec3(p.operation.v,0,0)) // velocity of car vs ground! (wind excluded)
  ));
  cm.insert(new PressureOutletBC(cm, "outlet", boundaryDict));
  cm.insert(new VelocityInletBC(cm, "inlet", boundaryDict, VelocityInletBC::Parameters()
      .set_velocity( FieldData::uniformSteady(p.operation.v,0,0) )
      .set_turbulence(turbulenceBC::turbulenceBCPtr(new turbulenceBC::uniformIntensityAndLengthScale(
           turbulenceBC::uniformIntensityAndLengthScale::Parameters()
            .set_I(turbI)
            .set_l(turbL)
        )))
  ));

  cm.addRemainingBCs<WallBC>(boundaryDict, WallBC::Parameters() );

  insertTurbulenceModel(cm,
    parameters_.get<SelectableSubsetParameter>("fluid/turbulenceModel"));

}




ResultSetPtr NumericalWindtunnel::evaluateResults(OpenFOAMCase& cm)
{
  Parameters p(parameters_);
  
  ResultSetPtr results=insight::OpenFOAMAnalysis::evaluateResults(cm);
  
  // get full name of car patch (depends on STL file)
  OFDictData::dict boundaryDict;
  cm.parseBoundaryDict(executionPath(), boundaryDict);
  std::string carPatchName;
  for (const OFDictData::dict::value_type& de: boundaryDict)
  {
    if (starts_with(de.first, "object"))
    {
      carPatchName=de.first;
      break;
    }
  }

  arma::mat Ah=projectedArea(cm, executionPath(), 
    vec3(1,0,0),
    list_of<std::string>("object")
  );
  double A=Ah(Ah.n_rows-1,1);

  
  double Re=p.operation.v*Lref_/p.fluid.nu;
  ptr_map_insert<ScalarResult>(*results) ("Re", Re, "Reynolds number", "", "");
  ptr_map_insert<ScalarResult>(*results) ("Afront", A, "Projected frontal area", "", "$m^2$");
    
  arma::mat f=forces::readForces(cm, executionPath(), "forces");
  arma::mat t = f.col(0);
  
  arma::mat Rtot = (f.col(1)+f.col(4));  
  arma::mat Rtlat = (f.col(2)+f.col(5));  
  arma::mat At = (f.col(3)+f.col(6));  
  
  ptr_map_insert<ScalarResult>(*results) ("Rtot", Rtot(Rtot.n_rows-1), "Total resistance", "", "N");
  ptr_map_insert<ScalarResult>(*results) ("Rtlat", Rtlat(Rtlat.n_rows-1), "Lateral force", "", "N");
  ptr_map_insert<ScalarResult>(*results) ("At", At(At.n_rows-1), "Lifting force", "", "N");

  double cw=Rtot(Rtot.n_rows-1) / (0.5*p.fluid.rho*pow(p.operation.v,2)*A);
  ptr_map_insert<ScalarResult>(*results) ("cw", cw, "Resistance coefficient", "", "");
  
  double ca=At(At.n_rows-1) / (0.5*p.fluid.rho*pow(p.operation.v,2)*A);
  ptr_map_insert<ScalarResult>(*results) ("ca", ca, "Lifting coefficient", "with respect to projected frontal area and forward velocity", "");

  double cl=Rtlat(Rtlat.n_rows-1) / (0.5*p.fluid.rho*pow(p.operation.v,2)*A);
  ptr_map_insert<ScalarResult>(*results) ("cl", cl, "Lateral forces coefficient", "with respect to projected frontal area and forward velocity", "");

  double Pe=Rtot(Rtot.n_rows-1) * p.operation.v;
  ptr_map_insert<ScalarResult>(*results) ("Pe", Pe, "Effective power", "", "W");

  // Resistance convergence
  addPlot
  (
    results, executionPath(), "chartResistance",
    "Iteration", "F [N]",
    list_of
      (PlotCurve( arma::mat(join_rows(t, Rtot)),  "Fdtot", "w l lw 2 t 'Total resistance'"))
      (PlotCurve( arma::mat(join_rows(t, Rtlat)), "Flat", "w l lw 2 t 'Lateral force'"))
      (PlotCurve( arma::mat(join_rows(t, At)),    "FLift", "w l lw 2 t 'Lifting force'"))
      ,
    "Convergence history of resistance force"
  );    
  


  {
    paraview::Streamtracer::Parameters::seed_cloud_type cloud1, cloud2;
    cloud1.radius=cloud2.radius=0.1*Lref_;
    cloud1.center=vec3(0.5*l_, 0.5*w_, 0.5*h_);
    cloud2.center=vec3(0.5*l_, -0.5*w_, 0.5*h_);

    paraview::ParaviewVisualization::Parameters pvp;
    pvp.scenes = {

      paraview::PVScenePtr(new paraview::CustomPVScene(paraview::CustomPVScene::Parameters()
        .set_command(
           "obj=extractPatches(openfoam_case, 'object.*')\n"
           "floor=extractPatches(openfoam_case, 'floor.*')\n"
           "displayContour(obj, 'p', arrayType='CELL_DATA')\n"
           "displaySolid(floor, 0.1)\n"
        )
      )),

      paraview::PVScenePtr(new paraview::Streamtracer(paraview::Streamtracer::Parameters()
        .set_seed(cloud1)
        .set_dataset("openfoam_case[0]")
        .set_field("U")
        .set_maxLen(10.*Lref_)
        .set_name("st1")
      )),

      paraview::PVScenePtr(new paraview::Streamtracer(paraview::Streamtracer::Parameters()
        .set_seed(cloud2)
        .set_dataset("openfoam_case[0]")
        .set_field("U")
        .set_maxLen(10.*Lref_)
        .set_name("st2")
      )),

      paraview::PVScenePtr(new paraview::IsoView(paraview::IsoView::Parameters()
        .set_bbmin(vec3(0, -0.5*w_, 0))
        .set_bbmax(vec3(l_, 0.5*w_, h_))
        .set_e_ax(vec3(-1,0,0))
        .set_filename("streamlines.png")
      )),

      paraview::PVScenePtr(new paraview::CustomPVScene(paraview::CustomPVScene::Parameters()
        .set_command(
            "Hide(st1)\n"
            "Hide(st2)\n"
        )
      )),

      paraview::PVScenePtr(new paraview::IsoView(paraview::IsoView::Parameters()
        .set_bbmin(vec3(0, -0.5*w_, 0))
        .set_bbmax(vec3(l_, 0.5*w_, h_))
        .set_e_ax(vec3(-1,0,0))
        .set_filename("pressureContour.png")
      ))

    };

    paraview::ParaviewVisualization pv(pvp, executionPath());
    ResultSetPtr images = pv();
    results->insert ( "renderings", images );
  }


//  std::string init=
//    "cbi=loadOFCase('"+executionPath().string()+"')\n"
//    "prepareSnapshots()\n";
//  {
//    format pvec("[%g, %g, %g]");
//    std::string filename="wave_above.png";
//    runPvPython
//    (
//      cm, executionPath(), list_of<std::string>
//      (
//	init+
//	"import numpy as np\n"
	
//	"eb=extractPatches(cbi, 'car.*|fwheels.*|rwheels.*')\n"
//	"fl=extractPatches(cbi, 'floor')\n"
//	"Show(eb)\n"
//	"Show(fl)\n"
//	"displayContour(eb, 'p', arrayType='CELL_DATA', barpos=[0.5, 0.75], barorient=0)\n"
	
//        "st=StreamTracer(Input=cbi[0], Vectors=['U'], MaximumStreamlineLength="+lexical_cast<string>(10.*Lref_)+")\n"
//        "st.SeedType.Center="+str( pvec % (0.5*l_) % (0.5*w_) % (0.5*h_))+"\n"
//	"st.SeedType.Radius="+lexical_cast<string>(0.5*h_)+"\n"
//	"Show(st)\n"
//        "st2=StreamTracer(Input=cbi[0], Vectors=['U'], MaximumStreamlineLength="+lexical_cast<string>(10.*Lref_)+")\n"
//        "st2.SeedType.Center="+str( pvec % (0.5*l_) % (-0.5*w_) % (0.5*h_))+"\n"
//	"st2.SeedType.Radius="+lexical_cast<string>(0.5*h_)+"\n"
//	"Show(st2)\n"

//        "setCam("+str( pvec % (-Lref_) % (0.5*Lref_) % (0.33*Lref_))+
//                  ", ["+str(format("%g")%(0.5*l_))+",0,0], [0,0,1], "
//		  +str(format("%g") % (/*0.33*1e-3*L_*/ 1.0))+")\n"
//	"WriteImage('rightfrontview.png')\n"
//        "setCam("+str( pvec % (-Lref_) % (-0.5*Lref_) % (0.33*Lref_))+
//                  ", ["+str(format("%g")%(0.5*l_))+",0,0], [0,0,1], "
//		  +str(format("%g") % (/*0.33*1e-3*L_*/ 1.0))+")\n"
//	"WriteImage('leftfrontview.png')\n"

//        "setCam("+str( pvec % (2.*Lref_) % (0.5*Lref_) % (0.33*Lref_))+
//                  ", ["+str(format("%g")%(0.5*l_))+",0,0], [0,0,1], "
//		  +str(format("%g") % (/*0.33*1e-3*L_*/ 1.0))+")\n"
//	"WriteImage('rightrearview.png')\n"
//        "setCam("+str( pvec % (2.*Lref_) % (-0.5*Lref_) % (0.33*Lref_))+
//                  ", ["+str(format("%g")%(0.5*l_))+",0,0], [0,0,1], "
//		  +str(format("%g") % (/*0.33*1e-3*L_*/ 1.0))+")\n"
//	"WriteImage('leftrearview.png')\n"
//      )
//    );
//    results->insert("contourPressureLeftFront",
//      std::unique_ptr<Image>(new Image
//      (
//       executionPath(), "leftfrontview.png",
//      "Pressure distribution on car, view from left side ahead", ""
//    )));
//    results->insert("contourPressureRightFront",
//      std::unique_ptr<Image>(new Image
//      (
//       executionPath(), "rightfrontview.png",
//      "Pressure distribution on car, view from right side ahead", ""
//    )));
//    results->insert("contourPressureLeftRear",
//      std::unique_ptr<Image>(new Image
//      (
//       executionPath(), "leftrearview.png",
//      "Pressure distribution on car, view from left side rear", ""
//    )));
//    results->insert("contourPressureRightRear",
//      std::unique_ptr<Image>(new Image
//      (
//       executionPath(), "rightrearview.png",
//      "Pressure distribution on car, view from right side rear", ""
//    )));
//  }

  return results;
}




ParameterSet_VisualizerPtr NumericalWindtunnel_visualizer()
{
    return ParameterSet_VisualizerPtr( new NumericalWindtunnel_ParameterSet_Visualizer );
}

addStandaloneFunctionToStaticFunctionTable(Analysis, NumericalWindtunnel, visualizer, NumericalWindtunnel_visualizer);


void NumericalWindtunnel_ParameterSet_Visualizer::recreateVisualizationElements(UsageTracker* ut)
{
  CurrentExceptionContext ec("Creating visualization of numerical wind tunnel parameters");

  CAD_ParameterSet_Visualizer::recreateVisualizationElements(ut);

  Parameters p(ps_);
  NumericalWindtunnel nwt(ps_, "");
  nwt.calcDerivedInputData();


  std::string geom_file_ext = p.geometry.objectfile.filename().extension().string();
  boost::to_lower(geom_file_ext);

  try
  {
    cad::FeaturePtr org_geom;

    if (geom_file_ext==".stl" || geom_file_ext==".stlb")
    {
      org_geom = cad::STL::create_trsf(p.geometry.objectfile, nwt.cad_to_cfd_);
    }
    else
    {
      org_geom = cad::Transform::create_trsf(
                   cad::Feature::CreateFromFile(p.geometry.objectfile.string()),
                   nwt.cad_to_cfd_
                   );
    }

    addFeature("object", org_geom);

    double Lupstream = nwt.Lref_*p.geometry.LupstreamByL;
    double Ldownstream = nwt.Lref_*p.geometry.LdownstreamByL;
    double Lup = nwt.Lref_*p.geometry.LupByH;
    double Laside = nwt.Lref_*p.geometry.LasideByW;

    addFeature
    (
      "domain",
       cad::Box::create(
        cad::matconst(vec3( -Lupstream, -0.5*nwt.w_-Laside, 0)),
        cad::matconst(vec3( Lupstream+nwt.l_+Ldownstream, 0, 0)),
        cad::matconst(vec3( 0, 0, Lup)),
        cad::matconst(vec3( 0, 2.*Laside+nwt.w_, 0))
       ),
       DisplayStyle::Wireframe
    );
  }
  catch (...)
  {
    // ignore
  }
}

    
}
