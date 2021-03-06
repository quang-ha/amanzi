/*
  WhetStone, version 2.1
  Release name: naka-to.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Virtual framework for L2 and H1 projectors. 
*/

#ifndef AMANZI_WHETSTONE_PROJECTORS_HH_
#define AMANZI_WHETSTONE_PROJECTORS_HH_

#include "Teuchos_RCP.hpp"

namespace Amanzi {
namespace WhetStone {

class DenseVector;
class VectorPolynomial;

class Projectors { 
 public:
  enum class Type {
    L2,
    H1,
  };

 public:
  explicit Projectors() {};
  ~Projectors() {};

  // elliptic projector
  virtual void H1Cell(
      int c, const std::vector<VectorPolynomial>& vf,
      const std::shared_ptr<DenseVector>& moments, VectorPolynomial& uc) {
    Errors::Message msg("Elliptic projector is not supported for this scheme.");
    Exceptions::amanzi_throw(msg);
  }

  // L2 projector 
  virtual void L2Cell(
      int c, const std::vector<VectorPolynomial>& vf,
      const std::shared_ptr<DenseVector>& moments, VectorPolynomial& uc) {
    Errors::Message msg("L2 projector is not supported for this scheme.");
    Exceptions::amanzi_throw(msg);
  }
};

}  // namespace WhetStone
}  // namespace Amanzi

#endif

