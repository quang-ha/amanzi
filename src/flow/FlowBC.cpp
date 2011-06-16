#include "FlowBC.hpp"

#include "float.h"

namespace Amanzi
{

FlowBC::FlowBC(Teuchos::ParameterList &list, 
	       const Teuchos::RCP<AmanziMesh::Mesh> &mesh) : mesh_(mesh)
{
  int nbc = list.get<int>("number of BCs", INT_MAX);

  // We can't handle more than 100 BC parameter lists.
  if (nbc > 100) throw std::exception();

  bc_.resize(nbc);

  for (int i = 0; i < nbc; ++i) {

    // By convention, boundary conditions have the name "BCXX"
    // where XX is a number from 00 to 99.
    std::string bc_name = "BC  ";

    int i1 = i%10;
    int i10 = i/10;

    bc_name[3] = '0' + i1;
    bc_name[2] = '0' + i10;

    if (!list.isSublist(bc_name)) throw std::exception();
    Teuchos::ParameterList bc_param = list.sublist(bc_name);

    // Get the Set ID and verify that it is valid.
    bc_[i].SetID = bc_param.get<int>("Side set ID", INT_MAX);
    if (bc_[i].SetID == INT_MAX) throw std::exception();
    if (!mesh->valid_set_id(bc_[i].SetID, AmanziMesh::FACE)) throw std::exception();

    // Get the corresponding list of (local) face IDs.
    bc_[i].Faces.resize(mesh->get_set_size(bc_[i].SetID, AmanziMesh::FACE, AmanziMesh::USED));
    mesh->get_set(bc_[i].SetID, AmanziMesh::FACE, AmanziMesh::USED, bc_[i].Faces.begin(), bc_[i].Faces.end());

    // Get the BC type and check it against the list of defined types.
    std::string type = bc_param.get<std::string>("Type", "NOT DEFINED");
    if (type == "NOT DEFINED") throw std::exception();
    bool read_value, need_aux;
    if (type == "Pressure Constant") {
      bc_[i].Type = PRESSURE_CONSTANT;
      need_aux = true;
      read_value = true;
    }
    else if (type == "No Flow") {
      bc_[i].Type = NO_FLOW;
      need_aux = false;
      read_value = false;
    }
    else if (type == "Darcy Constant") {
      bc_[i].Type = DARCY_CONSTANT;
      need_aux = false;
      read_value = true;
    }
    else if (type == "Static Head") {
      bc_[i].Type = STATIC_HEAD;
      need_aux = true;
      read_value = true;
    }
    else if (type == "Time Dependent Pressure Constant" ) {
      bc_[i].Type = TIME_DEPENDENT_PRESSURE_CONSTANT;
      need_aux = true;
      read_value = true;
    }
    else { 
      throw std::exception();
    }

    // Temp storage needed for Dirichlet-type conditions.
    if (need_aux) bc_[i].Aux.resize(bc_[i].Faces.size());

    // Get the BC data value if required.
    if (read_value) {
      bc_[i].Value = bc_param.get<double>("BC value", DBL_MAX);
      if (bc_[i].Value == DBL_MAX) throw std::exception();

      if (type == "Time Dependent Pressure Constant") {
	bc_[i].InitialValue = bc_param.get<double>("Initial BC value", DBL_MAX);
	if (bc_[i].InitialValue == DBL_MAX) throw std::exception();
	
	bc_[i].InitialTime = bc_param.get<double>("Initial Time",DBL_MAX);
	if (bc_[i].InitialTime == DBL_MAX) throw std::exception();

	bc_[i].FinalTime = bc_param.get<double>("Final Time",DBL_MAX);
	if (bc_[i].FinalTime == DBL_MAX) throw std::exception();
	
      }
    }
  }
}

} // close namespace Amanzi
