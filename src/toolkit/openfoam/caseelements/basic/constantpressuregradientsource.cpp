#include "constantpressuregradientsource.h"

#include "openfoam/ofdicts.h"
#include "openfoam/openfoamdict.h"

namespace insight {


defineType(ConstantPressureGradientSource);
addToOpenFOAMCaseElementFactoryTable(ConstantPressureGradientSource);

ConstantPressureGradientSource::ConstantPressureGradientSource( OpenFOAMCase& c, const ParameterSet& ps )
: OpenFOAMCaseElement(c, "ConstantPressureGradientSource", ps),
  p_(ps)
{
}

void ConstantPressureGradientSource::addIntoDictionaries(OFdicts& dictionaries) const
{
  if (OFversion()>=230)
  {
    OFDictData::dict& controlDict=dictionaries.lookupDict("system/controlDict");
    controlDict.getList("libs").insertNoDuplicate( "\"libconstantPressureGradient.so\"" );

    OFDictData::dict coeffs;
    OFDictData::list flds; flds.push_back("U");
    coeffs["fieldNames"]=flds;
    coeffs["gradP"]=OFDictData::dimensionedData("gradP", OFDictData::dimension(0, 1, -2, 0, 0, 0, 0), OFDictData::vector3(p_.gradp));

    OFDictData::dict fod;
    fod["type"]="constantPressureGradientExplicitSource";
    fod["active"]=true;
    if (OFversion()>=400)
    {
        coeffs["selectionMode"]="all";
    }
    else
    {
        fod["selectionMode"]="all";
    }
    fod["constantPressureGradientExplicitSourceCoeffs"]=coeffs;

    OFDictData::dict& fvOptions=dictionaries.lookupDict("system/fvOptions");
    fvOptions[name()]=fod;
  }
  else
  {
    throw insight::Exception("constantPressureGradient unavailable!");
    // for channelFoam:
 //   OFDictData::dict& transportProperties=dictionaries.lookupDict("constant/transportProperties");
 //   transportProperties["Ubar"]=OFDictData::dimensionedData("Ubar", dimVelocity, OFDictData::vector3(p_.Ubar()));
  }
}





} // namespace insight
