#ifndef BOUNDARYCONDITION_TURBULENCE_H
#define BOUNDARYCONDITION_TURBULENCE_H

#include "base/boost_include.h"
#include "base/parameterset.h"
#include "base/resultset.h"
#include "openfoam/openfoamcase.h"


namespace insight
{


namespace turbulenceBC
{


class turbulenceBC
{
public:
    declareType ( "turbulenceBC" );
    declareDynamicClass(turbulenceBC);

    virtual ~turbulenceBC();


    virtual void setDirichletBC_k(OFDictData::dict& BC, double U) const =0;
    virtual void setDirichletBC_omega(OFDictData::dict& BC, double U) const =0;
    virtual void setDirichletBC_epsilon(OFDictData::dict& BC, double U) const =0;
    virtual void setDirichletBC_nuTilda(OFDictData::dict& BC, double U) const =0;
    virtual void setDirichletBC_R(OFDictData::dict& BC, double U) const =0;
};


typedef std::shared_ptr<turbulenceBC> turbulenceBCPtr;



class uniformIntensityAndLengthScale
: public turbulenceBC
{
public:
#include "boundarycondition_turbulence__uniformIntensityAndLengthScale__Parameters.h"
/*
PARAMETERSET>>> uniformIntensityAndLengthScale Parameters

I = double 0.05 "Fluctuation intensity as fraction of mean velocity"
l = double 0.1 "Length scale"

<<<PARAMETERSET
*/

protected:
    Parameters p_;

public:
    declareType("uniformIntensityAndLengthScale");
    uniformIntensityAndLengthScale(const ParameterSet& ps);
    inline static turbulenceBCPtr create(const ParameterSet& ps) { return turbulenceBCPtr(new uniformIntensityAndLengthScale(ps)); }

    ParameterSet getParameters() const override { return p_; }

    void setDirichletBC_k(OFDictData::dict& BC, double U) const override;
    void setDirichletBC_omega(OFDictData::dict& BC, double U) const override;
    void setDirichletBC_epsilon(OFDictData::dict& BC, double U) const override;
    void setDirichletBC_nuTilda(OFDictData::dict& BC, double U) const override;
    void setDirichletBC_R(OFDictData::dict& BC, double U) const override;
};



}


}

#endif // BOUNDARYCONDITION_TURBULENCE_H
