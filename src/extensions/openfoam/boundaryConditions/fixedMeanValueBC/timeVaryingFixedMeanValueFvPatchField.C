/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright held by original author
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

\*---------------------------------------------------------------------------*/

#include "timeVaryingFixedMeanValueFvPatchField.H"
#include "mathematicalConstants.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

template<class Type>
timeVaryingFixedMeanValueFvPatchField<Type>::timeVaryingFixedMeanValueFvPatchField
(
    const fvPatch& p,
    const DimensionedField<Type, volMesh>& iF
)
:
    fixedValueFvPatchField<Type>(p, iF),
    meanValue_(),
    curTimeIndex_(-1)
{}


template<class Type>
timeVaryingFixedMeanValueFvPatchField<Type>::timeVaryingFixedMeanValueFvPatchField
(
    const timeVaryingFixedMeanValueFvPatchField<Type>& ptf,
    const fvPatch& p,
    const DimensionedField<Type, volMesh>& iF,
    const fvPatchFieldMapper& mapper
)
:
    fixedValueFvPatchField<Type>(ptf, p, iF, mapper),
    meanValue_(
            ptf.meanValue_.valid()
          ?

            #if defined(OF16ext)
            new DATAENTRY<Type>(ptf.meanValue_())
            #else
            ptf.meanValue_().clone().ptr()
            #endif

          : nullptr
),
    curTimeIndex_(-1)
{}


template<class Type>
timeVaryingFixedMeanValueFvPatchField<Type>::timeVaryingFixedMeanValueFvPatchField
(
    const fvPatch& p,
    const DimensionedField<Type, volMesh>& iF,
    const dictionary& dict
)
:
    fixedValueFvPatchField<Type>(p, iF),
    #if defined(OF16ext)
    meanValue_(new DATAENTRY<Type>(dict)),
    #else
    meanValue_(DATAENTRY<Type>::New("meanValue", dict)),
    #endif
    //meanValue_(pTraits<Type>(dict.lookup("meanValue"))),
    curTimeIndex_(-1)
{
    if (dict.found("value"))
    {
        fixedValueFvPatchField<Type>::operator==
        (
            Field<Type>("value", dict, p.size())
        );
    }
    else
    {
        const scalar t = this->db().time().timeOutputValue();
        Type meanValue__ =
        #if defined(OF16ext)
                meanValue_()(t)
        #else
                meanValue_->value(t)
        #endif
                ;
        fixedValueFvPatchField<Type>::operator==(meanValue__);
    }
}


template<class Type>
timeVaryingFixedMeanValueFvPatchField<Type>::timeVaryingFixedMeanValueFvPatchField
(
    const timeVaryingFixedMeanValueFvPatchField<Type>& ptf,
    const DimensionedField<Type, volMesh>& iF
)
:
    fixedValueFvPatchField<Type>(ptf, iF),
    meanValue_(
            ptf.meanValue_.valid()
          ?
            #if defined(OF16ext)
            new DATAENTRY<Type>(ptf.meanValue_())
            #else
            ptf.meanValue_().clone().ptr()
            #endif

          : nullptr
),
    curTimeIndex_(-1)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

// Map from self
template<class Type>
void timeVaryingFixedMeanValueFvPatchField<Type>::autoMap
(
    const fvPatchFieldMapper& m
)
{
    Field<Type>::autoMap(m);
}


// Reverse-map the given fvPatchField onto this fvPatchField
template<class Type>
void timeVaryingFixedMeanValueFvPatchField<Type>::rmap
(
    const fvPatchField<Type>& ptf,
    const labelList& addr
)
{
    fixedValueFvPatchField<Type>::rmap(ptf, addr);
}


// Update the coefficients associated with the patch field
template<class Type>
void timeVaryingFixedMeanValueFvPatchField<Type>::updateCoeffs()
{
    if (this->updated())
    {
        return;
    }

    if (curTimeIndex_ != this->db().time().timeIndex())
    {
        const scalar t = this->db().time().timeOutputValue();
        Type meanValue__ =
        #if defined(OF16ext)
                meanValue_()(t)
        #else
                meanValue_->value(t)
        #endif
                ;

        Info<<"meanValue at time " << t << ": " << meanValue__ <<endl;

        Field<Type>& patchField = *this;

        Field<Type> pif = this->patchInternalField();

        patchField = meanValue__ - gAverage(pif) + pif;

        curTimeIndex_ = this->db().time().timeIndex();
    }

    fixedValueFvPatchField<Type>::updateCoeffs();
}


// Write
template<class Type>
void timeVaryingFixedMeanValueFvPatchField<Type>::write(Ostream& os) const
{
    fvPatchField<Type>::write(os);
#if defined(OF16ext)
    meanValue_->write(os)
#else
    meanValue_->writeData(os)
#endif
    ;
//        << meanValue_ << token::END_STATEMENT << nl;
    this->writeEntry("value", os);
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
