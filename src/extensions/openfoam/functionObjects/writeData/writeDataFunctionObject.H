/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Typedef
    Foam::writeDataFunctionObject

Description
    FunctionObject wrapper around writeData to allow it to be created via
    the functions entry within controlDict.

SourceFiles
    writeDataFunctionObject.C

\*---------------------------------------------------------------------------*/

#ifndef writeDataFunctionObject_H
#define writeDataFunctionObject_H

#include "writeData.H"
#if OF_VERSION<040000 //not (defined(OFdev)||defined(OFplus)||defined(OFesi1806))
#include "OutputFilterFunctionObject.H"
#endif

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{
    typedef 
#if OF_VERSION>=040000 //defined(OFdev)||defined(OFplus)||defined(OFesi1806)
	writeData
#else
        OutputFilterFunctionObject<writeData>
#endif
        writeDataFunctionObject;
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
