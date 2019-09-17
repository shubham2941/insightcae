#include "dynamicmeshcaseelements.h"


using namespace std;
using namespace boost;
using namespace boost::filesystem;
using namespace boost::assign;
using namespace boost::fusion;



namespace insight
{




dynamicMesh::dynamicMesh(OpenFOAMCase& c, const ParameterSet& ps)
: OpenFOAMCaseElement(c, "dynamicMesh", ps)
{
}

bool dynamicMesh::isUnique() const
{
  return true;
}





velocityTetFEMMotionSolver::velocityTetFEMMotionSolver(OpenFOAMCase& c)
: dynamicMesh(c, ParameterSet()),
  tetFemNumerics_(c)
{
  c.addField("motionU", FieldInfo(vectorField, 	dimVelocity, 		FieldValue({0.0, 0.0, 0.0}), tetField ) );
}

void velocityTetFEMMotionSolver::addIntoDictionaries(OFdicts& dictionaries) const
{
  tetFemNumerics_.addIntoDictionaries(dictionaries);

  OFDictData::dict& tetFemSolution=dictionaries.lookupDict("system/tetFemSolution");
  OFDictData::dict& solvers = tetFemSolution.subDict("solvers");
  solvers["motionU"]=OFcase().stdSymmSolverSetup();

  OFDictData::dict& dynamicMeshDict=dictionaries.lookupDict("constant/dynamicMeshDict");
  dynamicMeshDict["dynamicFvMesh"]=OFDictData::data("dynamicMotionSolverFvMesh");
  dynamicMeshDict["solver"]=OFDictData::data("laplaceFaceDecomposition");
  if (dynamicMesh::OFversion()<=160)
  {
    dynamicMeshDict["diffusivity"]=OFDictData::data("uniform");
    dynamicMeshDict["frozenDiffusion"]=OFDictData::data(false);
    dynamicMeshDict["twoDMotion"]=OFDictData::data(false);
  }
  else
  {
    throw insight::Exception("No tetFEMMotionsolver available for OF>1.6 ext");
  }
}





displacementFvMotionSolver::displacementFvMotionSolver(OpenFOAMCase& c)
: dynamicMesh(c, ParameterSet())
{
}

void displacementFvMotionSolver::addIntoDictionaries(OFdicts& dictionaries) const
{
  OFDictData::dict& dynamicMeshDict=dictionaries.lookupDict("constant/dynamicMeshDict");
  dynamicMeshDict["dynamicFvMesh"]=OFDictData::data("dynamicMotionSolverFvMesh");
  dynamicMeshDict["solver"]=OFDictData::data("displacementLaplacian");
  if (OFversion()<220)
  {
    dynamicMeshDict["diffusivity"]=OFDictData::data("uniform");
  }
  else
  {
    OFDictData::dict sd;
    sd["diffusivity"]=OFDictData::data("uniform");
    dynamicMeshDict["displacementLaplacianCoeffs"]=sd;
  }
}




defineType(solidBodyMotionDynamicMesh);
addToOpenFOAMCaseElementFactoryTable(solidBodyMotionDynamicMesh);



solidBodyMotionDynamicMesh::solidBodyMotionDynamicMesh( OpenFOAMCase& c, const ParameterSet& ps )
: dynamicMesh(c, ps),
  ps_(ps)
{
}


void solidBodyMotionDynamicMesh::addIntoDictionaries(OFdicts& dictionaries) const
{
    Parameters p(ps_);

    OFDictData::dict& dynamicMeshDict
      = dictionaries.lookupDict("constant/dynamicMeshDict");

    dynamicMeshDict["dynamicFvMesh"]="dynamicMotionSolverFvMesh";
    dynamicMeshDict["solver"]="solidBody";
    OFDictData::dict sbc;

    sbc["cellZone"]=p.zonename;

    if ( Parameters::motion_rotation_type* rp = boost::get<Parameters::motion_rotation_type>(&p.motion) )
    {
        sbc["solidBodyMotionFunction"]="rotatingMotion";
        OFDictData::dict rmc;
        rmc["origin"]=OFDictData::vector3(rp->origin);
        rmc["axis"]=OFDictData::vector3(rp->axis);
        rmc["omega"]=2.*M_PI*rp->rpm/60.;
        sbc["rotatingMotionCoeffs"]=rmc;
    }
    else if ( Parameters::motion_oscillatingRotating_type* ro = boost::get<Parameters::motion_oscillatingRotating_type>(&p.motion) )
    {
        sbc["solidBodyMotionFunction"]="oscillatingRotatingMotion";
        OFDictData::dict rmc;
        rmc["origin"]=OFDictData::vector3(ro->origin);
        rmc["omega"]=ro->omega;
        rmc["amplitude"]=OFDictData::vector3(ro->amplitude);
        sbc["oscillatingRotatingMotionCoeffs"]=rmc;
    }
    else
      throw insight::Exception("Internal error: Unhandled selection!");

    dynamicMeshDict["solidBodyCoeffs"]=sbc;
}




defineType(rigidBodyMotionDynamicMesh);
addToOpenFOAMCaseElementFactoryTable(rigidBodyMotionDynamicMesh);



rigidBodyMotionDynamicMesh::rigidBodyMotionDynamicMesh( OpenFOAMCase& c, const ParameterSet& ps )
: dynamicMesh(c, ps),
  ps_(ps)
{
}

void rigidBodyMotionDynamicMesh::addFields( OpenFOAMCase& c ) const
{
  c.addField
  (
      "pointDisplacement",
       FieldInfo(vectorField, 	dimLength, 	FieldValue({0, 0, 0}), pointField )
  );
}

void rigidBodyMotionDynamicMesh::addIntoDictionaries(OFdicts& dictionaries) const
{
    Parameters p(ps_);

    OFDictData::dict& dynamicMeshDict
      = dictionaries.lookupDict("constant/dynamicMeshDict");

    std::string name="rigidBodyMotion";

    dynamicMeshDict["dynamicFvMesh"]="dynamicMotionSolverFvMesh";
    OFDictData::dict rbmc;

    if (const auto* impl = boost::get<Parameters::implementation_extended_type>(&p.implementation))
    {
      name="extendedRigidBodyMotion";
      rbmc["rampDuration"]=impl->rampDuration;
    }

    dynamicMeshDict["solver"]=name;


    OFDictData::list libl;
    libl.push_back("\"lib"+name+".so\"");
    dynamicMeshDict["motionSolverLibs"]=libl;

    rbmc["report"]=true;


    OFDictData::dict sc;
     sc["type"]="Newmark";
    rbmc["solver"]=sc;

    if (const Parameters::rho_field_type* rhof = boost::get<Parameters::rho_field_type>(&p.rho))
      {
        rbmc["rho"]=rhof->fieldname;
      }
    else if (const Parameters::rho_constant_type* rhoc = boost::get<Parameters::rho_constant_type>(&p.rho))
      {
        rbmc["rho"]="rhoInf";
        rbmc["rhoInf"]=rhoc->rhoInf;
      }

    rbmc["accelerationRelaxation"]=0.4;

    OFDictData::dict bl;
    for (const Parameters::bodies_default_type& body: p.bodies)
    {
      OFDictData::dict bc;

      bc["type"]="rigidBody";
      bc["parent"]="root";
      bc["centreOfMass"]=OFDictData::vector3(0,0,0);
      bc["mass"]=body.mass;
      bc["inertia"]=boost::str(boost::format("(%g 0 0 %g 0 %g)") % body.Ixx % body.Iyy % body.Izz);
      bc["transform"]=boost::str(boost::format("(1 0 0 0 1 0 0 0 1) (%g %g %g)")
                                 % body.centreOfMass(0) % body.centreOfMass(1) % body.centreOfMass(2) );

      OFDictData::list jl;
      for (const Parameters::bodies_default_type::translationConstraint_default_type& tc:
                    body.translationConstraint)
      {
        std::string code;
        if (tc==Parameters::bodies_default_type::translationConstraint_default_type::Px) code="Px";
        else if (tc==Parameters::bodies_default_type::translationConstraint_default_type::Py) code="Py";
        else if (tc==Parameters::bodies_default_type::translationConstraint_default_type::Pz) code="Pz";
        else if (tc==Parameters::bodies_default_type::translationConstraint_default_type::Pxyz) code="Pxyz";
        else throw insight::Exception("internal error: unhandled value!");
        OFDictData::dict d;
         d["type"]=code;
        jl.push_back(d);
      }
      for (const Parameters::bodies_default_type::rotationConstraint_default_type& rc:
                    body.rotationConstraint)
      {
        std::string code;
        if (rc==Parameters::bodies_default_type::rotationConstraint_default_type::Rx) code="Rx";
        else if (rc==Parameters::bodies_default_type::rotationConstraint_default_type::Ry) code="Ry";
        else if (rc==Parameters::bodies_default_type::rotationConstraint_default_type::Rz) code="Rz";
        else if (rc==Parameters::bodies_default_type::rotationConstraint_default_type::Rxyz) code="Rxyz";
        else throw insight::Exception("internal error: unhandled value!");
        OFDictData::dict d;
         d["type"]=code;
        jl.push_back(d);
      }
      OFDictData::dict jc;
       jc["type"]="composite";
       jc["joints"]=jl;
      bc["joint"]=jc;

      OFDictData::list pl;
      std::copy(body.patches.begin(), body.patches.end(), std::back_inserter(pl));
      bc["patches"]=pl;

      bc["innerDistance"]=body.innerDistance;
      bc["outerDistance"]=body.outerDistance;

      bl[body.name]=bc;
    }
    rbmc["bodies"]=bl;

    OFDictData::dict rc;
     // empty
    rbmc["restraints"]=rc;

    dynamicMeshDict[name+"Coeffs"]=rbmc;
}



}
