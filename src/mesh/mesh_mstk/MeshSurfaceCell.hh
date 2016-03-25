/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
//
// This is a mesh for a single surface cell.
//
// This exists solely because we need "surface meshes" extracted from
// MeshColumn.  This is really just 1 cell.  Really.
//

#ifndef AMANZI_MESH_SURFACE_CELL_HH_
#define AMANZI_MESH_SURFACE_CELL_HH_

#include <vector>
#include <string>
#include <algorithm>

#include "Teuchos_ParameterList.hpp"
#include "Epetra_Map.h"
#include "Epetra_MpiComm.h"
#include "Epetra_SerialComm.h"

#include "VerboseObject.hh"
#include "dbc.hh"
#include "errors.hh"

#include "Region.hh"
#include "Mesh_MSTK.hh"

namespace Amanzi {
namespace AmanziMesh {

class MeshSurfaceCell : public Mesh {

 public:

  MeshSurfaceCell(const Mesh& inmesh,
                  const std::string& surface_set_name,
                  const Teuchos::RCP<const VerboseObject>& vo = Teuchos::null,
                  bool flatten=true)
      : Mesh(vo, true, false),
        parent_mesh_(inmesh) {
    // set comm
    set_comm(inmesh.get_comm());

    // set dimensions
    if (flatten) {
      set_space_dimension(2);
    } else {
      set_space_dimension(3);
    }
    set_manifold_dimension(2);

    // set my face
    Entity_ID_List my_face;
    inmesh.get_set_entities(surface_set_name, FACE, OWNED, &my_face);
    ASSERT(my_face.size() == 1);
    parent_face_ = my_face[0];

    // set my nodes
    Entity_ID_List my_nodes;
    inmesh.face_get_nodes(parent_face_, &my_nodes);
    nodes_.resize(my_nodes.size());
    for (int i=0; i!=my_nodes.size(); ++i) {
      inmesh.node_get_coordinates(my_nodes[i], &nodes_[i]);
    }

    // set the maps
    cell_map_ = Teuchos::rcp(new Epetra_Map(1, 0, *get_comm()));
    face_map_ = Teuchos::rcp(new Epetra_Map((int)nodes_.size(), 0, *get_comm()));
    exterior_face_importer_ =
        Teuchos::rcp(new Epetra_Import(*face_map_,*face_map_));

    // set the geometric model and sets
    Teuchos::RCP<const AmanziGeometry::GeometricModel> gm = inmesh.geometric_model();
    set_geometric_model(gm);
    for (AmanziGeometry::GeometricModel::RegionConstIterator r=gm->RegionBegin();
         r!=gm->RegionEnd(); ++r) {

      // set to false as default
      sets_[(*r)->id()] = false;
      
      // determine if true
      if ((*r)->type() == AmanziGeometry::LABELEDSET
          || (*r)->type() == AmanziGeometry::ENUMERATED) {
        // label pulled from parent
        Entity_ID_List faces_in_set;
        inmesh.get_set_entities((*r)->id(), FACE, OWNED, &faces_in_set);
        sets_[(*r)->id()] = std::find(faces_in_set.begin(), faces_in_set.end(),
                parent_face_) != faces_in_set.end();

      } else if ((*r)->is_geometric()) {
        // check containment
        if ((*r)->space_dimension() == 3) {
          sets_[(*r)->id()] = (*r)->inside(inmesh.face_centroid(parent_face_));

        } else if ((*r)->space_dimension() == 2 && flatten) {
          sets_[(*r)->id()] = (*r)->inside(cell_centroid(0));
        }
      }      
    }

    // set the cell type
    if (nodes_.size() == 3) {
      cell_type_ = TRI;
    } else if (nodes_.size() == 4) {
      cell_type_ = QUAD;
    } else {
      cell_type_ = POLYGON;
    }
  }

  ~MeshSurfaceCell() {}

  // Get parallel type of entity - OWNED, GHOST, USED (See MeshDefs.hh)
  virtual
  Parallel_type entity_get_ptype(const Entity_kind kind,
          const Entity_ID entid) const {
    return OWNED;
  }

  // Parent entity in the source mesh if mesh was derived from another mesh
  virtual
  Entity_ID entity_get_parent(const Entity_kind kind, const Entity_ID entid) const {
    ASSERT(kind == CELL);
    ASSERT(entid == 0);
    return parent_face_;
  }

  // Get cell type - UNKNOWN, TRI, QUAD, ... See MeshDefs.hh
  virtual
  Cell_type cell_get_type(const Entity_ID cellid) const {
    return cell_type_;
  }


  //
  // General mesh information
  // -------------------------
  //

  // Number of entities of any kind (cell, face, node) and in a
  // particular category (OWNED, GHOST, USED)
  virtual
  unsigned int num_entities(const Entity_kind kind,
                            const Parallel_type ptype) const {
    int count;
    switch (kind) {
      case CELL:
        count = 1;
        break;

      default: // num_nodes == num_faces == num_boundary_faces
        count = nodes_.size();
        break;
    }
    return count;
  }


  // Global ID of any entity
  virtual
  Entity_ID GID(const Entity_ID lid, const Entity_kind kind) const {
    return lid;
  }


  //
  // Mesh Entity Adjacencies
  //-------------------------

  // Downward Adjacencies
  //---------------------

  // Get nodes of a cell
  virtual
  void cell_get_nodes(const Entity_ID cellid,
                      Entity_ID_List *nodeids) const {
    ASSERT(cellid == 0);
    ASSERT(nodeids);
    nodeids->resize(nodes_.size());
    for (int i=0; i!=nodes_.size(); ++i) (*nodeids)[i] = i;
  }


  // Get nodes of face
  // On a distributed mesh, all nodes (OWNED or GHOST) of the face
  // are returned
  // In 3D, the nodes of the face are returned in ccw order consistent
  // with the face normal
  // In 2D, nfnodes is 2
  virtual
  void face_get_nodes(const Entity_ID faceid,
                      Entity_ID_List *nodeids) const {
    ASSERT(faceid < nodes_.size());
    nodeids->resize(2);
    (*nodeids)[0] = faceid;
    (*nodeids)[1] = (faceid + 1) % nodes_.size();
  }


  // Get nodes of edge
  virtual
  void edge_get_nodes(const Entity_ID edgeid,
                      Entity_ID *nodeid0, Entity_ID *nodeid1) const {
    Errors::Message mesg("Not implemented");
    amanzi_throw(mesg);
  }


  // Upward adjacencies
  //-------------------

  // Cells of type 'ptype' connected to a node - The order of cells is
  // not guaranteed to be the same for corresponding nodes on
  // different processors
  virtual
  void node_get_cells(const Entity_ID nodeid,
                      const Parallel_type ptype,
                      Entity_ID_List *cellids) const {
    cellids->resize(1);
    (*cellids)[0] = 0;
  }


  // Faces of type 'ptype' connected to a node - The order of faces is
  // not guarnateed to be the same for corresponding nodes on
  // different processors
  virtual
  void node_get_faces(const Entity_ID nodeid,
                      const Parallel_type ptype,
                      Entity_ID_List *faceids) const {
    Errors::Message mesg("Not implemented");
    amanzi_throw(mesg);
  }


  // Get faces of ptype of a particular cell that are connected to the
  // given node - The order of faces is not guarnateed to be the same
  // for corresponding nodes on different processors
  virtual
  void node_get_cell_faces(const Entity_ID nodeid,
                           const Entity_ID cellid,
                           const Parallel_type ptype,
                           Entity_ID_List *faceids) const {
    Errors::Message mesg("Not implemented");
    amanzi_throw(mesg);
  }

  // Same level adjacencies
  //-----------------------

  // Face connected neighboring cells of given cell of a particular ptype
  // (e.g. a hex has 6 face neighbors)

  // The order in which the cellids are returned cannot be
  // guaranteed in general except when ptype = USED, in which case
  // the cellids will correcpond to cells across the respective
  // faces given by cell_get_faces
  virtual
  void cell_get_face_adj_cells(const Entity_ID cellid,
          const Parallel_type ptype,
          Entity_ID_List *fadj_cellids) const {
    fadj_cellids->resize(0);
  }


  // Node connected neighboring cells of given cell
  // (a hex in a structured mesh has 26 node connected neighbors)
  // The cells are returned in no particular order
  virtual
  void cell_get_node_adj_cells(const Entity_ID cellid,
          const Parallel_type ptype,
          Entity_ID_List *nadj_cellids) const {
    nadj_cellids->resize(0);
  }



  //
  // Mesh entity geometry
  //--------------
  //

  // Node coordinates - 3 in 3D and 2 in 2D
  virtual
  void node_get_coordinates(const Entity_ID nodeid,
                            AmanziGeometry::Point *ncoord) const {
    (*ncoord) = nodes_[nodeid];
  }


  // Face coordinates - conventions same as face_to_nodes call
  // Number of nodes is the vector size divided by number of spatial dimensions
  virtual
  void face_get_coordinates(const Entity_ID faceid,
                            std::vector<AmanziGeometry::Point> *fcoords) const {
    fcoords->resize(2);
    (*fcoords)[0] = nodes_[faceid];
    (*fcoords)[1] = nodes_[(faceid + 1) % nodes_.size()];
  }

  // Coordinates of cells in standard order (Exodus II convention)
  // STANDARD CONVENTION WORKS ONLY FOR STANDARD CELL TYPES IN 3D
  // For a general polyhedron this will return the node coordinates in
  // arbitrary order
  // Number of nodes is vector size divided by number of spatial dimensions
  virtual
  void cell_get_coordinates(const Entity_ID cellid,
                            std::vector<AmanziGeometry::Point> *ccoords) const {
    (*ccoords) = nodes_;
  }


  //
  // Mesh modification
  //-------------------

  // Set coordinates of node
  virtual
  void node_set_coordinates(const Entity_ID nodeid,
                            const AmanziGeometry::Point ncoord) {
    nodes_[nodeid] = ncoord;
  }


  virtual
  void node_set_coordinates(const Entity_ID nodeid,
                            const double *ncoord) {
    
    Errors::Message mesg("Not implemented");
    Exceptions::amanzi_throw(mesg);
  }


  // Deform a mesh so that cell volumes conform as closely as possible
  // to target volumes without dropping below the minimum volumes.  If
  // move_vertical = true, nodes will be allowed to move only in the
  // vertical direction (right now arbitrary node movement is not allowed)
  // Nodes in any set in the fixed_sets will not be permitted to move.
  virtual
  int deform(const std::vector<double>& target_cell_volumes_in,
             const std::vector<double>& min_cell_volumes_in,
             const Entity_ID_List& fixed_nodes,
             const bool move_vertical) {
    Errors::Message mesg("Not implemented");
    Exceptions::amanzi_throw(mesg);
  }

  //
  // Epetra maps
  //------------
  virtual
  const Epetra_Map& cell_map(bool include_ghost) const {
    return *cell_map_;
  }

  virtual
  const Epetra_Map& face_map(bool include_ghost) const {
    return *face_map_;
  }

  // dummy implementation so that frameworks can skip or overwrite
  const Epetra_Map& edge_map(bool include_ghost) const
  {
    Errors::Message mesg("Edges not implemented in this framework");
    amanzi_throw(mesg);
  };

  virtual
  const Epetra_Map& node_map(bool include_ghost) const {
    return *face_map_;
  }

  virtual
  const Epetra_Map& exterior_face_map(bool include_ghost) const {
    return *face_map_;
  }


  // Epetra importer that will allow apps to import values from a
  // Epetra vector defined on all owned faces into an Epetra vector
  // defined only on exterior faces
  virtual
  const Epetra_Import& exterior_face_importer(void) const {
    return *exterior_face_importer_;
  }


  //
  // Mesh Sets for ICs, BCs, Material Properties and whatever else
  //--------------------------------------------------------------
  //

  // Get number of entities of type 'category' in set
  virtual
  unsigned int get_set_size(const Set_ID setid,
                            const Entity_kind kind,
                            const Parallel_type ptype) const {
    if (sets_.at(setid)) {
      return kind == CELL ? 1 : nodes_.size();
    }
    return 0;
  }


  virtual
  unsigned int get_set_size(const std::string setname,
                            const Entity_kind kind,
                            const Parallel_type ptype) const {
    return get_set_size(geometric_model()->FindRegion(setname)->id(), kind, ptype);
  }

  virtual
  unsigned int get_set_size(const char *setname,
                            const Entity_kind kind,
                            const Parallel_type ptype) const {
    std::string setname1(setname);
    return get_set_size(setname1,kind,ptype);
  }


  // Get list of entities of type 'category' in set
  virtual
  void get_set_entities(const Set_ID setid,
                        const Entity_kind kind,
                        const Parallel_type ptype,
                        Entity_ID_List *entids) const {
    if (sets_.at(setid)) {
      if (kind == CELL) {
        entids->resize(1,0);
      } else {
        entids->resize(nodes_.size());
        for (int i=0; i!=nodes_.size(); ++i) (*entids)[i] = i;
      }
    } else {
      entids->resize(0);
    }
  }

  virtual
  void get_set_entities(const std::string setname,
                        const Entity_kind kind,
                        const Parallel_type ptype,
                        Entity_ID_List *entids) const {
    return get_set_entities(geometric_model()->FindRegion(setname)->id(),
                            kind, ptype, entids);
  }

  virtual
  void get_set_entities(const char *setname,
                        const Entity_kind kind,
                        const Parallel_type ptype,
                        Entity_ID_List *entids) const {
    std::string setname1(setname);
    get_set_entities(setname1,kind,ptype,entids);
  }


  // Miscellaneous functions
  virtual
  void write_to_exodus_file(const std::string filename) const {
    Errors::Message mesg("Not implemented");
    Exceptions::amanzi_throw(mesg);
  }


 protected:

  // get faces and face dirs of a cell. This can be called by
  // cell_get_faces_and_dirs method of the base class and the data
  // cached or it can be called directly by the
  // cell_get_faces_and_dirs method of this class
  virtual
  void cell_get_faces_and_dirs_internal_(const Entity_ID cellid,
          Entity_ID_List *faceids,
          std::vector<int> *face_dirs,
          const bool ordered=false) const {
    ASSERT(cellid == 0);
    faceids->resize(nodes_.size());
    for (int i=0; i!=nodes_.size(); ++i) (*faceids)[i] = i;
    face_dirs->resize(nodes_.size(),1);
  }


  // Cells connected to a face - this function is implemented in each
  // mesh framework. The results are cached in the base class
  virtual
  void face_get_cells_internal_(const Entity_ID faceid,
          const Parallel_type ptype,
          Entity_ID_List *cellids) const {
    cellids->resize(1,0);
  }


  // edges of a face - this function is implemented in each mesh
  // framework. The results are cached in the base class
  virtual
  void face_get_edges_and_dirs_internal_(const Entity_ID faceid,
          Entity_ID_List *edgeids,
          std::vector<int> *edge_dirs,
          const bool ordered=true) const {
    Errors::Message mesg("Not implemented");
    Exceptions::amanzi_throw(mesg);
  }

  // edges of a cell - this function is implemented in each mesh
  // framework. The results are cached in the base class.
  virtual
  void cell_get_edges_internal_(const Entity_ID cellid,
          Entity_ID_List *edgeids) const {
    Errors::Message mesg("Not implemented");
    Exceptions::amanzi_throw(mesg);
  }


  // edges and directions of a 2D cell - this function is implemented
  // in each mesh framework. The results are cached in the base class.
  virtual
  void cell_2D_get_edges_and_dirs_internal_(const Entity_ID cellid,
          Entity_ID_List *edgeids,
          std::vector<int> *edge_dirs) const {
    Errors::Message mesg("Not implemented");
    Exceptions::amanzi_throw(mesg);
  }

 protected:

  Mesh const& parent_mesh_;

  std::vector<AmanziGeometry::Point> nodes_;
  std::map<Set_ID,bool> sets_;
  Entity_ID parent_face_;
  Cell_type cell_type_;

  Teuchos::RCP<Epetra_Map> cell_map_;
  Teuchos::RCP<Epetra_Map> face_map_;
  Teuchos::RCP<Epetra_Import> exterior_face_importer_;

};


} // close namespace AmanziMesh
} // close namespace Amanzi





#endif /* _MESH_MAPS_H_ */