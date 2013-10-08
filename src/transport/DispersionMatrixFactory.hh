/*
  This is the Transport component of Amanzi.

  License: BSD
  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#ifndef AMANZI_DISPERSION_MATRIX_FACTORY_HH_
#define AMANZI_DISPERSION_MATRIX_FACTORY_HH_

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Dispersion.hh"

namespace Amanzi {
namespace AmanziTransport {

class DispersionMatrixFactory {
 public:
  DispersionMatrixFactory() {};
  ~DispersionMatrixFactory() {};

  Teuchos::RCP<Dispersion> Create(
      const string& matrix_name, const Teuchos::ParameterList& matrix_list);
};

}  // namespace AmanziTransport
}  // namespace Amanzi

#endif
