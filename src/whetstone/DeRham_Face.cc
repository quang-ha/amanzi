/*
  WhetStone, version 2.1
  Release name: naka-to.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Derham complex: mimetic inner products on faces.
*/

#include "Mesh.hh"

#include "DeRham_Face.hh"
#include "WhetStone_typedefs.hh"

namespace Amanzi {
namespace WhetStone {

/* ******************************************************************
* Only upper triangular part of Mc = R (R^T N)^{-1} R^T is calculated.
****************************************************************** */
int DeRham_Face::L2consistency(
    int c, const Tensor& K, DenseMatrix& N, DenseMatrix& Mc, bool symmetry)
{
  Entity_ID_List faces;
  std::vector<int> dirs;
  mesh_->cell_get_faces_and_dirs(c, &faces, &dirs);

  int nfaces = faces.size();
  if (nfaces != N.NumRows()) return nfaces;  // matrix was not reshaped

  int d = mesh_->space_dimension();
  double volume = mesh_->cell_volume(c);

  AmanziGeometry::Point v1(d), v2(d);
  const AmanziGeometry::Point& cm = mesh_->cell_centroid(c);

  Tensor Kinv(K);
  Kinv.Inverse();
  Kinv.Transpose();

  for (int i = 0; i < nfaces; i++) {
    int f = faces[i];
    const AmanziGeometry::Point& fm = mesh_->face_centroid(f);
    double a1 = mesh_->face_area(f);
    v2 = Kinv * (fm - cm);

    int i0 = (symmetry ? i : 0);
    for (int j = i0; j < nfaces; j++) {
      f = faces[j];
      const AmanziGeometry::Point& fm = mesh_->face_centroid(f);
      double a2 = mesh_->face_area(f);
      v1 = fm - cm;
      Mc(i, j) = (v1 * v2) * (a1 * a2) / volume;
    }
  }

  for (int i = 0; i < nfaces; i++) {
    int f = faces[i];
    const AmanziGeometry::Point& normal = mesh_->face_normal(f);
    double area = mesh_->face_area(f);
    for (int k = 0; k < d; k++) N(i, k) = normal[k] * dirs[i] / area;
  }
  return WHETSTONE_ELEMENTAL_MATRIX_OK;
}

}  // namespace WhetStone
}  // namespace Amanzi


