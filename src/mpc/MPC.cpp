#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "MPC.hpp"
#include "State.hpp"
#include "Chemistry_State.hpp"
#include "Chemistry_PK.hpp"
#include "Flow_State.hpp"
#include "Flow_PK.hpp"
#include "Transport_State.hpp"
#include "Transport_PK.hpp"
#include "gmv_mesh.hh"
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"

MPC::MPC(Teuchos::ParameterList parameter_list_,
	 Teuchos::RCP<Mesh_maps_base> mesh_maps_):
  parameter_list(parameter_list_),
  mesh_maps(mesh_maps_)
  
{
   
   mpc_parameter_list =  parameter_list.sublist("MPC");
   read_parameter_list();

   Teuchos::ParameterList state_parameter_list = 
     parameter_list.sublist("State");

   // create the state object
   S = Teuchos::rcp( new State( state_parameter_list, mesh_maps) );
   
   // create auxilary state objects for the process models
   // chemistry...
   
   CS = Teuchos::rcp( new Chemistry_State( S ) );
   
   TS = Teuchos::rcp( new Transport_State( *S ) );

   FS = Teuchos::rcp( new Flow_State( S ) ); 
   // done creating auxilary state objects for the process models

  
   // create the individual process models
   // chemistry...
   Teuchos::ParameterList chemistry_parameter_list = 
     parameter_list.sublist("Chemistry");
   
   CPK = Teuchos::rcp( new Chemistry_PK(chemistry_parameter_list, CS) );
   
   // transport...
   Teuchos::ParameterList transport_parameter_list = 
     parameter_list.sublist("Transport");
   
   TPK = Teuchos::rcp( new Transport_PK(transport_parameter_list, TS) );
   
   // flow...
   Teuchos::ParameterList flow_parameter_list = 
     parameter_list.sublist("Flow");
   
   FPK = Teuchos::rcp( new Flow_PK(flow_parameter_list, FS) );
   // done creating the individual process models
}


void MPC::read_parameter_list()
{
  T0 = mpc_parameter_list.get<double>("Start Time");
  T1 = mpc_parameter_list.get<double>("End Time");
}


void MPC::cycle_driver () {
  
  // so far we only have transport working

  // start at time T=T0;
  S->set_time(T0);


  // get the GMV data from the parameter list
  Teuchos::ParameterList gmv_parameter_list =  mpc_parameter_list.sublist("GMV");
  
  string gmv_meshfile_ = gmv_parameter_list.get<string>("Mesh file name");
  string gmv_datafile_ = gmv_parameter_list.get<string>("Data file name");
  string gmv_prefix = gmv_parameter_list.get<string>("GMV prefix","./");
  
  // create the GMV subdirectory
  boost::filesystem::path prefix(gmv_prefix);
  if (!boost::filesystem::is_directory(prefix.directory_string())) {
    boost::filesystem::create_directory(prefix.directory_string());
  }
  boost::filesystem::path fmesh(gmv_meshfile_);
  boost::filesystem::path fdata(gmv_datafile_);
  boost::filesystem::path slash("/");

  std::stringstream  mesh_filename;
  mesh_filename << prefix.directory_string(); 
  mesh_filename << slash.directory_string();
  mesh_filename << fmesh.directory_string();

  string gmv_meshfile = mesh_filename.str();

  std::stringstream  data_filename;
  data_filename << prefix.directory_string(); 
  data_filename << slash.directory_string();
  data_filename << fdata.directory_string();

  string gmv_datafile = data_filename.str();

  const int gmv_cycle_freq = gmv_parameter_list.get<int>("Dump cycle frequency");
    
  // write the GMV mesh file
  GMV::create_mesh_file(*mesh_maps,gmv_meshfile);
  
  int iter = 0;

  // write the GMV data file
  write_mesh_data(gmv_meshfile, gmv_datafile, iter, 6);
  
  // first solve the flow equation
  if (parameter_list.get<string>("disable Flow_PK","no") == "no") {
    FPK->advance();
    S->update_darcy_flux(FPK->DarcyFlux());
    FPK->commit_state(FS);
  }
  

  // let users selectively disable individual process kernels
  // to allow for testing of the process kernels separately

  bool transport_disabled = 
    (parameter_list.get<string>("disable Transport_PK","no") == "yes");
  bool chemistry_disabled =
    (parameter_list.get<string>("disable Chemistry_PK","no") == "yes");
  
  cout << transport_disabled << " " << chemistry_disabled << endl;
  
  if (!chemistry_disabled || !transport_disabled) {
    
    // then iterate transport and chemistry
    while (S->get_time() <= T1) {
      double mpc_dT, chemistry_dT=1e+99, transport_dT=1e+99;

      if (!transport_disabled) transport_dT = TPK->calculate_transport_dT();

      mpc_dT = min( transport_dT, chemistry_dT );
      
      std::cout << "MPC: ";
      std::cout << "Cycle = " << iter; 
      std::cout << ",  Time = "<< S->get_time();
      std::cout << ",  Transport dT = " << transport_dT << std::endl;
      
      if (!transport_disabled) {
	// now advance transport
	TPK->advance( mpc_dT );	
	if (TPK->get_transport_status() == Amanzi_Transport::TRANSPORT_STATE_COMPLETE) 
	  {
	    // get the transport state and commit it to the state
	    RCP<Transport_State> TS_next = TPK->get_transport_state_next();
	    S->update_total_component_concentration(TS_next->get_total_component_concentration());
	  }
	else
	  {
	    // something went wrong
	    throw std::exception();
	  }
      }

      if (!chemistry_disabled) {
	// now advance chemistry
	chemistry_dT = transport_dT; // units?
	CPK->advance(chemistry_dT, total_component_concentration_star);
	Chemistry_PK::ChemistryStatus cpk_status = CPK->status();
      }

      // update the time in the state object
      S->advance_time(mpc_dT);
 

      // we're done with this time step, commit the state 
      // in the process kernels
      if (!transport_disabled) TPK->commit_state(TS);
      if (!chemistry_disabled) CPK->commit_state(CS, chemistry_dT);
	
     
      // advance the iteration count
      iter++;
      
      if (  iter % gmv_cycle_freq   == 0 ) {
	cout << "Writing GMV file..." << endl;
	write_mesh_data(gmv_meshfile, gmv_datafile, iter, 6);      
      }
      
    }
    
  }

}



void MPC::write_mesh_data(std::string gmv_meshfile, std::string gmv_datafile, 
			  const int iter, const int digits)
{
  
  GMV::open_data_file(gmv_meshfile, gmv_datafile,
		      mesh_maps->count_entities(Mesh_data::NODE, OWNED),
		      mesh_maps->count_entities(Mesh_data::CELL, OWNED),
		      iter, digits);
  GMV::write_time(S->get_time());
  GMV::write_cycle(iter);
  GMV::start_data();
  
  string basestring = "concentration";
  string suffix = ".00";
  
  for (int nc=0; nc<S->get_number_of_components(); nc++) {
    string concstring(basestring);
    GMV::suffix_no(suffix,nc);
    concstring.append(suffix);
    
    GMV::write_cell_data( *(*S->get_total_component_concentration())(nc), concstring);
  }
  
  // GMV::write_face_data( *(->get_darcy_flux()), "flux"); 
  
  GMV::close_data_file();     
 
}
