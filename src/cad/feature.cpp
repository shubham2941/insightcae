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

#include "feature.h"
#include "solidmodel.h"
#include "boost/lexical_cast.hpp"
#include "boost/foreach.hpp"

using namespace std;
using namespace boost;

namespace insight 
{
namespace cad 
{
  
FeatureSet::FeatureSet(const FeatureSet& o)
: model_(o.model_),
  shape_(o.shape_)
{
  insert(o.begin(), o.end());
}

  
FeatureSet::FeatureSet(const SolidModel& m, EntityType shape)
: model_(m),
  shape_(shape)
{
}

FeatureSet::operator TopAbs_ShapeEnum () const
{
  if (shape_==Edge) return TopAbs_EDGE;
  else if (shape_==Face) return TopAbs_FACE;
  else if (shape_==Vertex) return TopAbs_VERTEX;
  else throw insight::Exception("Unknown EntityType:"+lexical_cast<std::string>(shape_));
}


void FeatureSet::safe_union(const FeatureSet& o)
{
  if (o.shape_!=shape_)
    throw insight::Exception("incompatible shape type between feature sets!");
  else if (!(o.model_==model_))
    throw insight::Exception("feature sets belong to different models!");
  else
  {
    insert(o.begin(), o.end());
  }
}


std::auto_ptr<FeatureSet> FeatureSet::clone() const
{
  std::auto_ptr<FeatureSet> nfs(new FeatureSet(model_, shape_));
  nfs->insert(begin(), end());
  return nfs;
}

void FeatureSet::write() const
{
  std::cout<<'[';
  BOOST_FOREACH(FeatureID i, *this)
  {
    std::cout<<" "<<i;
  }
  std::cout<<" ]"<<std::endl;
}


  
}
}