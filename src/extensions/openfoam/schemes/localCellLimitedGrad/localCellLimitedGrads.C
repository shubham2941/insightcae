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

#include "localCellLimitedGrad.H"
#include "gaussGrad.H"
#include "fvMesh.H"
#include "volMesh.H"
#include "surfaceMesh.H"
#include "volFields.H"
#include "fixedValueFvPatchFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#if (OF_VERSION<=030000) //not (defined(OF301)||defined(OFplus)||defined(OFdev)||defined(OFesi1806))
namespace Foam
{
namespace fv
{
#endif
  
    makeFvGradScheme(localCellLimitedGrad)

#if (OF_VERSION<=030000) //not (defined(OF301)||defined(OFplus)||defined(OFdev)||defined(OFesi1806))
}
}
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //



template<>
Foam::tmp<Foam::volVectorField>
Foam::fv::localCellLimitedGrad<Foam::scalar>::
#if (defined(OF_FORK_extend) && OF_VERSION<010602) //(defined(OF16ext) && !defined(Fx32) && !defined(Fx40) && !defined(Fx41))
grad
#else
calcGrad
#endif
(
    const volScalarField& vsf
#if (defined(OF_FORK_extend) && OF_VERSION<010602) //(defined(OF16ext) && !defined(Fx32) && !defined(Fx40) && !defined(Fx41))
#else
    ,
    const word& name
#endif
) const
{
    const fvMesh& mesh = vsf.mesh();

    tmp<volVectorField> tGrad = basicGradScheme_().
#if (defined(OF_FORK_extend) && OF_VERSION<010602) //(defined(OF16ext) && !defined(Fx32) && !defined(Fx40) && !defined(Fx41))
    grad(vsf);
#else
    calcGrad(vsf, name);
#endif

//     if (k_ < SMALL)
//     {
//         return tGrad;
//     }

    volVectorField& g = UNIOF_TMP_NONCONST(tGrad);

    const UNIOF_LABELULIST& owner = mesh.owner();
    const UNIOF_LABELULIST& neighbour = mesh.neighbour();

    const volVectorField& C = mesh.C();
    const surfaceVectorField& Cf = mesh.Cf();

    scalarField maxVsf(vsf.internalField());
    scalarField minVsf(vsf.internalField());

    forAll(owner, facei)
    {
        label own = owner[facei];
        label nei = neighbour[facei];

        scalar vsfOwn = vsf[own];
        scalar vsfNei = vsf[nei];

        maxVsf[own] = max(maxVsf[own], vsfNei);
        minVsf[own] = min(minVsf[own], vsfNei);

        maxVsf[nei] = max(maxVsf[nei], vsfOwn);
        minVsf[nei] = min(minVsf[nei], vsfOwn);
    }

#if (OF_VERSION>=040000) //defined(OFplus)||defined(OFdev)||defined(OFesi1806)
    const volScalarField::Boundary&
#else
    const volScalarField::GeometricBoundaryField& 
#endif
     bsf = vsf.boundaryField();

    forAll(bsf, patchi)
    {
        const fvPatchScalarField& psf = bsf[patchi];

        const UNIOF_LABELULIST& pOwner = mesh.boundary()[patchi].faceCells();

        if (psf.coupled())
        {
            const scalarField psfNei(psf.patchNeighbourField());

            forAll(pOwner, pFacei)
            {
                label own = pOwner[pFacei];
                scalar vsfNei = psfNei[pFacei];

                maxVsf[own] = max(maxVsf[own], vsfNei);
                minVsf[own] = min(minVsf[own], vsfNei);
            }
        }
        else
        {
            forAll(pOwner, pFacei)
            {
                label own = pOwner[pFacei];
                scalar vsfNei = psf[pFacei];

                maxVsf[own] = max(maxVsf[own], vsfNei);
                minVsf[own] = min(minVsf[own], vsfNei);
            }
        }
    }

    maxVsf -= vsf;
    minVsf -= vsf;

//     if (k_ < 1.0)
    {
//       volScalarField k
//       (
// 	   IOobject
//             (
//                 "gradLimiterCoeff",
//                 mesh.time().timeName(),
//                 mesh,
//                 IOobject::NO_READ,
//                 IOobject::NO_WRITE
//             ),
//             mesh,
//             dimensionedScalar("k", dimless, 1.0)  // limit everywhere
//       );
//       if (mesh.foundObject<volScalarField>(kname_))
//       {
//         k = mesh.lookupObject<volScalarField>(kname_);
//       }      
        scalarField maxMinVsf((1.0/min(1., max(1e-3, getk(mesh))) - 1.0)*(maxVsf - minVsf));
        maxVsf += maxMinVsf;
        minVsf -= maxMinVsf;

        //maxVsf *= 1.0/k_;
        //minVsf *= 1.0/k_;
    }


    // create limiter
    scalarField limiter(vsf.internalField().size(), 1.0);

    forAll(owner, facei)
    {
        label own = owner[facei];
        label nei = neighbour[facei];

        // owner side
        limitFace
        (
            limiter[own],
            maxVsf[own],
            minVsf[own],
            (Cf[facei] - C[own]) & g[own]
        );

        // neighbour side
        limitFace
        (
            limiter[nei],
            maxVsf[nei],
            minVsf[nei],
            (Cf[facei] - C[nei]) & g[nei]
        );
    }

    forAll(bsf, patchi)
    {
        const UNIOF_LABELULIST& pOwner = mesh.boundary()[patchi].faceCells();
        const vectorField& pCf = Cf.boundaryField()[patchi];

        forAll(pOwner, pFacei)
        {
            label own = pOwner[pFacei];

            limitFace
            (
                limiter[own],
                maxVsf[own],
                minVsf[own],
                (pCf[pFacei] - C[own]) & g[own]
            );
        }
    }

    if (fv::debug)
    {
        Info<< "gradient limiter for: " << vsf.name()
            << " max = " << gMax(limiter)
            << " min = " << gMin(limiter)
            << " average: " << gAverage(limiter) << endl;
    }

    UNIOF_INTERNALFIELD_NONCONST(g) *= limiter;
      
    g.correctBoundaryConditions();
    gaussGrad<scalar>::correctBoundaryConditions(vsf, g);

    return tGrad;
}


template<>
Foam::tmp<Foam::volTensorField>
Foam::fv::localCellLimitedGrad<Foam::vector>::
#if (defined(OF_FORK_extend) && OF_VERSION<010602) //(defined(OF16ext) && !defined(Fx32) && !defined(Fx40) && !defined(Fx41))
grad
#else
calcGrad
#endif
(
    const volVectorField& vsf
#if (defined(OF_FORK_extend) && OF_VERSION<010602) //(defined(OF16ext) && !defined(Fx32) && !defined(Fx40) && !defined(Fx41))
#else
    ,
    const word& name
#endif
) const
{
    const fvMesh& mesh = vsf.mesh();

    tmp<volTensorField> tGrad = basicGradScheme_().
#if (defined(OF_FORK_extend) && OF_VERSION<010602) //(defined(OF16ext) && !defined(Fx32) && !defined(Fx40) && !defined(Fx41))
    grad(vsf);
#else
    calcGrad(vsf, name);
#endif

//     if (k_ < SMALL)
//     {
//         return tGrad;
//     }

    volTensorField& g = UNIOF_TMP_NONCONST(tGrad);

    const UNIOF_LABELULIST& owner = mesh.owner();
    const UNIOF_LABELULIST& neighbour = mesh.neighbour();

    const volVectorField& C = mesh.C();
    const surfaceVectorField& Cf = mesh.Cf();

    vectorField maxVsf(vsf.internalField());
    vectorField minVsf(vsf.internalField());

    forAll(owner, facei)
    {
        label own = owner[facei];
        label nei = neighbour[facei];

        const vector& vsfOwn = vsf[own];
        const vector& vsfNei = vsf[nei];

        maxVsf[own] = max(maxVsf[own], vsfNei);
        minVsf[own] = min(minVsf[own], vsfNei);

        maxVsf[nei] = max(maxVsf[nei], vsfOwn);
        minVsf[nei] = min(minVsf[nei], vsfOwn);
    }

#if (OF_VERSION>=040000) //defined(OFplus)||defined(OFdev)||defined(OFesi1806)
    const volVectorField::Boundary&
#else
    const volVectorField::GeometricBoundaryField& 
#endif
      bsf = vsf.boundaryField();

    forAll(bsf, patchi)
    {
        const fvPatchVectorField& psf = bsf[patchi];
        const UNIOF_LABELULIST& pOwner = mesh.boundary()[patchi].faceCells();

        if (psf.coupled())
        {
            const vectorField psfNei(psf.patchNeighbourField());

            forAll(pOwner, pFacei)
            {
                label own = pOwner[pFacei];
                const vector& vsfNei = psfNei[pFacei];

                maxVsf[own] = max(maxVsf[own], vsfNei);
                minVsf[own] = min(minVsf[own], vsfNei);
            }
        }
        else
        {
            forAll(pOwner, pFacei)
            {
                label own = pOwner[pFacei];
                const vector& vsfNei = psf[pFacei];

                maxVsf[own] = max(maxVsf[own], vsfNei);
                minVsf[own] = min(minVsf[own], vsfNei);
            }
        }
    }

    maxVsf -= vsf;
    minVsf -= vsf;

//     if (k_ < 1.0)
    {
//       volScalarField k
//       (
// 	   IOobject
//             (
//                 "gradLimiterCoeff",
//                 mesh.time().timeName(),
//                 mesh,
//                 IOobject::NO_READ,
//                 IOobject::NO_WRITE
//             ),
//             mesh,
//             dimensionedScalar("k", dimless, 1.0)  // limit everywhere
//       );
//       if (mesh.foundObject<volScalarField>(kname_))
//       {
//         k = mesh.lookupObject<volScalarField>(kname_);
//       }

      const vectorField maxMinVsf((1.0/min(1., max(1e-3, getk(mesh))) - 1.0)*(maxVsf - minVsf));
        maxVsf += maxMinVsf;
        minVsf -= maxMinVsf;

        //maxVsf *= 1.0/k_;
        //minVsf *= 1.0/k_;
    }


    // create limiter
    vectorField limiter(vsf.internalField().size(), vector::one);

    forAll(owner, facei)
    {
        label own = owner[facei];
        label nei = neighbour[facei];

        // owner side
        limitFace
        (
            limiter[own],
            maxVsf[own],
            minVsf[own],
            (Cf[facei] - C[own]) & g[own]
        );

        // neighbour side
        limitFace
        (
            limiter[nei],
            maxVsf[nei],
            minVsf[nei],
            (Cf[facei] - C[nei]) & g[nei]
        );
    }

    forAll(bsf, patchi)
    {
        const UNIOF_LABELULIST& pOwner = mesh.boundary()[patchi].faceCells();
        const vectorField& pCf = Cf.boundaryField()[patchi];

        forAll(pOwner, pFacei)
        {
            label own = pOwner[pFacei];

            limitFace
            (
                limiter[own],
                maxVsf[own],
                minVsf[own],
                ((pCf[pFacei] - C[own]) & g[own])
            );
        }
    }

    if (fv::debug)
    {
        Info<< "gradient limiter for: " << vsf.name()
            << " max = " << gMax(limiter)
            << " min = " << gMin(limiter)
            << " average: " << gAverage(limiter) << endl;
    }

    tensorField& gIf = UNIOF_INTERNALFIELD_NONCONST(g);

    forAll(gIf, celli)
    {
        gIf[celli] = tensor
        (
            cmptMultiply(limiter[celli], gIf[celli].x()),
            cmptMultiply(limiter[celli], gIf[celli].y()),
            cmptMultiply(limiter[celli], gIf[celli].z())
        );
    }

    g.correctBoundaryConditions();
    gaussGrad<vector>::correctBoundaryConditions(vsf, g);

    return tGrad;
}


// ************************************************************************* //
