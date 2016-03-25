/*
  A region consisting of all entities on the domain boundary

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#ifndef AMANZI_REGION_BOUNDARY_HH_
#define AMANZI_REGION_BOUNDARY_HH_

#include <vector>

#include "errors.hh"
#include "GeometryDefs.hh"
#include "Region.hh"

namespace Amanzi {
namespace AmanziGeometry {

class RegionBoundary : public Region {
public:

  RegionBoundary(const std::string& name,
	         const Set_ID id,
	         const LifeCycleType lifecycle=PERMANENT);

  // Is the the specified point inside this region
  bool inside(const Point& p) const;
  
protected:
  const std::string entity_str_; // what kind of entities make up this set
  const std::vector<Entity_ID> entities_; // list of those included
};

} // namespace AmanziGeometry
} // namespace Amanzi

#endif