/*
  This is the flow component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon (ecoon@lanl.gov)

  A collection of WRMs along with a mesh partition.
*/

#ifndef AMANZI_FLOW_WRM_PARTITION_HH_
#define AMANZI_FLOW_WRM_PARTITION_HH_

#include "Mesh.hh"
#include "MeshPartition.hh"
#include "WRM.hh"

namespace Amanzi {
namespace Flow {

typedef std::vector<Teuchos::RCP<WRM> > WRMList;
typedef std::pair<Teuchos::RCP<Functions::MeshPartition>, WRMList> WRMPartition;

// Non-member factory
Teuchos::RCP<WRMPartition> CreateWRMPartition(
    Teuchos::RCP<const AmanziMesh::Mesh>& mesh,
    Teuchos::RCP<Teuchos::ParameterList> plist);

}  // namespace Flow
}  // namespace Amanzi

#endif