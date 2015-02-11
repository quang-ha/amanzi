/*
  License: see $AMANZI_DIR/COPYRIGHT
  Authors: Daniil Svyatskiy

  Dummy PK which demonstrates the require interface for PK
  BDFFnBase<CompositeVector>, to use TreeVectors.

*/


#include "Dummy_PK.hh"
#include <iostream>


namespace Amanzi {


Dummy_PK::Dummy_PK(Teuchos::ParameterList& pk_tree,
        const Teuchos::RCP<Teuchos::ParameterList>& global_list,
        const Teuchos::RCP<State>& S,
        const Teuchos::RCP<TreeVector>& soln) :
    S_(S),
    soln_(soln)
{
  // Expect a single global list with sublist Flow
  glist_ = Teuchos::rcp(new Teuchos::ParameterList(*global_list));
  //std::cout << "pk_tree name: " << pk_tree.name() << "\n";
  //glist_->set("Flow", global_list->sublist("PKs").sublist(pk_tree.name()));

  ti_list_ = glist_->sublist("Cycle Driver").sublist("time intervals").sublist("TI 0");
  
  // construct a Multiphase_PK
  //pk_ = Teuchos::rcp(new Flow::Multiphase_PK(*glist_, S_));
}

bool Dummy_PK::AdvanceStep(double t_old, double t_new) {

  bool failed = false;
  
  double dt = t_new - t_old;

  return failed;
}

}