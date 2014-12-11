/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2014  <copyright holder> <email>
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

#ifndef INSIGHT_FLATPLATEBL_H
#define INSIGHT_FLATPLATEBL_H

#include "openfoam/openfoamanalysis.h"

namespace insight {

class FlatPlateBL 
: public OpenFOAMAnalysis
{
  
protected:

    double Cw_, delta2e_, H_, W_, Re_theta2e_, uinf_, ypfac_e_, deltaywall_e_, gradh_, T_;
    int nax_, nlat_;
    
    double avgStart_, avg2Start_, end_;
    
    std::string in_, out_, top_, cycl_prefix_;
  
public:
  declareType("Flat Plate Boundary Layer Test Case");
  
  FlatPlateBL(const NoParameters&);
  ~FlatPlateBL();
  
  virtual ParameterSet defaultParameters() const;
  virtual void calcDerivedInputData(const ParameterSet& p);
  virtual void createCase(insight::OpenFOAMCase& cm, const insight::ParameterSet& p);
  virtual void createMesh(insight::OpenFOAMCase& cm, const insight::ParameterSet& p);

  virtual void evaluateAtSection
  (
    OpenFOAMCase& cm, const ParameterSet& p, 
    ResultSetPtr results, double x, int i
  );  
  virtual insight::ResultSetPtr evaluateResults(insight::OpenFOAMCase& cm, const insight::ParameterSet& p);
  
  virtual insight::Analysis* clone();
  
  /**
   * solves the function G(Alpha,D) numerically
   */
  static double G(double Alpha, double D);
  
  /**
   * computes the friction coefficient of a flat plate
   * @Re Reynolds number formulated with running length
   */
  static double cw(double Re, double Cplus=5.0);

  /**
   * computes the friction coefficient of a flat plate at station x
   * @Re Reynolds number formulated with running distance x
   */
  static double cf(double Re, double Cplus=5.0);
  
  static double integrateDelta2(const arma::mat& uByUinf_vs_y);
};

}

#endif // INSIGHT_FLATPLATEBL_H
