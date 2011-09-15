#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Epetra_MpiComm.h"
#include "State.hpp"
#include "chemistry_state.hh"
#include "chemistry_pk.hh"
#include "Transport_State.hpp"
#include "Transport_PK.hpp"
#include "Flow_State.hpp"
#include "Flow_PK.hpp"
#include "ObservationData.H"
#include "Unstructured_observations.hpp"
#include "Vis.hpp"

namespace Amanzi
{

class MPC {

public:
  MPC (Teuchos::ParameterList parameter_list_,
       Teuchos::RCP<AmanziMesh::Mesh> mesh_maps_,
       Epetra_MpiComm* comm_,
       Amanzi::ObservationData& output_observations_); 

  ~MPC () {};

  void cycle_driver ();

private:
  void mpc_init();
  void read_parameter_list();
  
  // states
  Teuchos::RCP<State> S;
  Teuchos::RCP<amanzi::chemistry::Chemistry_State> CS;
  Teuchos::RCP<AmanziTransport::Transport_State> TS; 
  Teuchos::RCP<Flow_State> FS;

  // misc setup information
  Teuchos::ParameterList parameter_list;
  Teuchos::RCP<AmanziMesh::Mesh> mesh_maps;

  // storage for the component concentration intermediate values
  Teuchos::RCP<Epetra_MultiVector> total_component_concentration_star;

  // process kernels
  Teuchos::RCP<amanzi::chemistry::Chemistry_PK> CPK;
  Teuchos::RCP<AmanziTransport::Transport_PK> TPK;
  Teuchos::RCP<Flow_PK> FPK; 

  Teuchos::ParameterList mpc_parameter_list;

  double T0, T1;
  int end_cycle;

  bool flow_enabled, transport_enabled, chemistry_enabled;

  // these are the vectors that chemistry will populate with
  // the names for the auxillary output vectors and the
  // names of components
  std::vector<string> auxnames;
  std::vector<string> compnames; 

  std::string flow_model;
  std::string restart_file;
  bool restart;

  // Epetra communicator
  Epetra_MpiComm* comm;

  // observations
  Amanzi::ObservationData&  output_observations;
  Amanzi::Unstructured_observations* observations;

  // visualization
  Amanzi::Vis *visualization;

};


} // close namespace Amanzi
