/*
  This is the flow component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon (ecoon@lanl.gov)

  Field evaluator for total volumetric water content which is the 
  conserved quantity in the Richards equation.

  Wrapping this conserved quantity as a field evaluator makes it
  easier to take derivatives, keep updated, and the like.
  The equation for this is simply:

    VWC = phi * (s_liquid * n_liquid + X_gas * s_gas * n_gas)

  where X_gas is the molar fraction of water in the gas phase.
*/


#ifndef AMANZI_FLOW_VWCONTENT_EVALUATOR_HH_
#define AMANZI_FLOW_VWCONTENT_EVALUATOR_HH_

#include "Teuchos_ParameterList.hpp"

#include "factory.hh"
#include "secondary_variable_field_evaluator.hh"

namespace Amanzi {
namespace Flow {

class VWContentEvaluator : public SecondaryVariableFieldEvaluator {
 public:
  explicit VWContentEvaluator(Teuchos::ParameterList& plist);
  VWContentEvaluator(const VWContentEvaluator& other);

  virtual Teuchos::RCP<FieldEvaluator> Clone() const;

  virtual void Init_();

  // Required methods from SecondaryVariableFieldEvaluator
  virtual void EvaluateField_(const Teuchos::Ptr<State>& S,
          const Teuchos::Ptr<CompositeVector>& result);
  virtual void EvaluateFieldPartialDerivative_(const Teuchos::Ptr<State>& S,
          Key wrt_key, const Teuchos::Ptr<CompositeVector>& result);

 protected:
  bool vapor_phase_;
  Teuchos::ParameterList plist_;
  
 private:
  static Utils::RegisteredFactory<FieldEvaluator,VWContentEvaluator> reg_;
};

}  // namespace Flow
}  // namespace Amanzi

#endif
