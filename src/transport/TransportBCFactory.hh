/*
  This is the transport component of the Amanzi code.

  Copyright 2010-2013 held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#ifndef AMANZI_TRANSPORT_BC_FACTORY_HH_
#define AMANZI_TRANSPORT_BC_FACTORY_HH_

#include <vector>

#include "Mesh.hh"
#include "TransportBoundaryFunction.hh"
#include "TransportBoundaryFunction_Tracer.hh"
#include "TransportBoundaryFunction_Alquimia.hh"

namespace Amanzi {
namespace AmanziTransport {

class TransportBCFactory {
 public:
  TransportBCFactory(const Teuchos::RCP<const AmanziMesh::Mesh>& mesh,
                     const Teuchos::RCP<Teuchos::ParameterList>& list)
     : mesh_(mesh), list_(list) {};
  ~TransportBCFactory() {};
  
  void CreateConcentration(std::vector<TransportBoundaryFunction*>& bcs) const;

  // non-reactive components
  void ProcessTracerList(std::vector<TransportBoundaryFunction*>& bcs) const;
  void ProcessTracerSpec(
      Teuchos::ParameterList& spec, TransportBoundaryFunction_Tracer* bc) const;

  // reactive components
  void ProcessGeochemicalConditionList(std::vector<TransportBoundaryFunction*>& bcs) const {};
  void ProcessGeochemicalConditionSpec(
      Teuchos::ParameterList& spec, TransportBoundaryFunction_Alquimia* bc) const {};

 private:
  const Teuchos::RCP<const AmanziMesh::Mesh>& mesh_;
  const Teuchos::RCP<Teuchos::ParameterList>& list_;
};

}  // namespace AmanziTransport
}  // namespace Amanzi

#endif