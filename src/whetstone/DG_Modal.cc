/*
  WhetStone, version 2.1
  Release name: naka-to.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Discontinuous Galerkin methods.
*/

#include "Point.hh"

#include "BasisFactory.hh"
#include "DenseMatrix.hh"
#include "DG_Modal.hh"
#include "MeshUtils.hh"
#include "Polynomial.hh"
#include "VectorPolynomial.hh"
#include "WhetStoneDefs.hh"

namespace Amanzi {
namespace WhetStone {

/* ******************************************************************
* Constructor.
****************************************************************** */
DG_Modal::DG_Modal(int order, Teuchos::RCP<const AmanziMesh::Mesh> mesh, std::string basis_name)
  : numi_(mesh, false),
    order_(order), 
    mesh_(mesh),
    d_(mesh_->space_dimension())
{
  int ncells_wghost = mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::Parallel_type::ALL);
  basis_.resize(ncells_wghost);

  BasisFactory factory;
  for (int c = 0; c < ncells_wghost; ++c) {
    basis_[c] = factory.Create(basis_name);
    basis_[c]->Init(mesh_, c, order_);
  }
}


/* ******************************************************************
* Mass matrix for Taylor basis functions. 
****************************************************************** */
int DG_Modal::MassMatrix(int c, const Tensor& K, DenseMatrix& M)
{
  double K00 = K(0, 0);

  // extend list of integrals of monomials
  UpdateIntegrals_(c, 2 * order_);
  const Polynomial& integrals = integrals_[c];
   
  // copy integrals to mass matrix
  int multi_index[3];
  Polynomial p(d_, order_);

  int nrows = p.size();
  M.Reshape(nrows, nrows);

  for (auto it = p.begin(); it.end() <= p.end(); ++it) {
    const int* idx_p = it.multi_index();
    int k = it.PolynomialPosition();

    for (auto jt = it; jt.end() <= p.end(); ++jt) {
      const int* idx_q = jt.multi_index();
      int l = jt.PolynomialPosition();
      
      int n(0);
      for (int i = 0; i < d_; ++i) {
        multi_index[i] = idx_p[i] + idx_q[i];
        n += multi_index[i];
      }

      M(l, k) = M(k, l) = K00 * integrals(n, p.MonomialSetPosition(multi_index));
    }
  }

  basis_[c]->ChangeBasisMatrix(M);

  return 0;
}


/* ******************************************************************
* Mass matrix for Taylor basis functions. 
****************************************************************** */
int DG_Modal::MassMatrix(
    int c, const Tensor& K, PolynomialOnMesh& integrals, DenseMatrix& M)
{
  double K00 = K(0, 0);

  // extend list of integrals of monomials
  numi_.UpdateMonomialIntegralsCell(c, 2 * order_, integrals);
   
  // copy integrals to mass matrix
  int multi_index[3];
  Polynomial p(d_, order_);

  int nrows = p.size();
  M.Reshape(nrows, nrows);

  for (auto it = p.begin(); it.end() <= p.end(); ++it) {
    const int* idx_p = it.multi_index();
    int k = it.PolynomialPosition();

    for (auto jt = it; jt.end() <= p.end(); ++jt) {
      const int* idx_q = jt.multi_index();
      int l = jt.PolynomialPosition();
      
      int n(0);
      for (int i = 0; i < d_; ++i) {
        multi_index[i] = idx_p[i] + idx_q[i];
        n += multi_index[i];
      }

      M(k, l) = K00 * integrals.poly()(n, p.MonomialSetPosition(multi_index));
      M(l, k) = M(k, l);
    }
  }

  basis_[c]->ChangeBasisMatrix(M);

  return 0;
}


/* ******************************************************************
* Mass matrix for Taylor basis and polynomial coefficient K.
****************************************************************** */
int DG_Modal::MassMatrixPoly_(int c, const Polynomial& K, DenseMatrix& M)
{
  // rebase the polynomial
  Polynomial Kcopy(K);
  Kcopy.ChangeOrigin(mesh_->cell_centroid(c));
  numi_.ChangeBasisRegularToNatural(c, Kcopy);

  // extend list of integrals of monomials
  int uk(Kcopy.order());
  UpdateIntegrals_(c, 2 * order_ + uk);
  const Polynomial& integrals = integrals_[c];
   
  // sum up integrals to the mass matrix
  int multi_index[3];
  Polynomial p(d_, order_);

  int nrows = p.size();
  M.Reshape(nrows, nrows);
  M.PutScalar(0.0);

  for (auto it = p.begin(); it.end() <= p.end(); ++it) {
    const int* idx_p = it.multi_index();
    int k = it.PolynomialPosition();

    for (auto mt = Kcopy.begin(); mt.end() <= Kcopy.end(); ++mt) {
      const int* idx_K = mt.multi_index();
      int m = mt.MonomialSetPosition();
      double factor = Kcopy(mt.end(), m);
      if (factor == 0.0) continue;

      for (auto jt = it; jt.end() <= p.end(); ++jt) {
        const int* idx_q = jt.multi_index();
        int l = jt.PolynomialPosition();

        int n(0);
        for (int i = 0; i < d_; ++i) {
          multi_index[i] = idx_p[i] + idx_q[i] + idx_K[i];
          n += multi_index[i];
        }

        M(k, l) += factor * integrals(n, p.MonomialSetPosition(multi_index));
      }
    }
  }

  // symmetric part of mass matrix
  for (int k = 0; k < nrows; ++k) {
    for (int l = k + 1; l < nrows; ++l) {
      M(l, k) = M(k, l); 
    }
  }

  basis_[c]->ChangeBasisMatrix(M);

  return 0;
}


/* ******************************************************************
* Mass matrix for Taylor basis and piecewise polynomial coefficient K.
****************************************************************** */
int DG_Modal::MassMatrixPiecewisePoly_(
    int c, const VectorPolynomial& K, DenseMatrix& M)
{
  AmanziMesh::Entity_ID_List faces, nodes;
  mesh_->cell_get_faces(c, &faces);
  int nfaces = faces.size();

  const AmanziGeometry::Point& xc = mesh_->cell_centroid(c);

  // allocate memory for matrix
  Polynomial p(d_, order_);
  int nrows = p.size();
  M.Reshape(nrows, nrows);
  M.PutScalar(0.0);

  std::vector<const Polynomial*> polys(3);

  std::vector<AmanziGeometry::Point> xy(3); 
  // xy[0] = cell_geometric_center(*mesh_, c);
  xy[0] = mesh_->cell_centroid(c);

  for (auto it = p.begin(); it.end() <= p.end(); ++it) {
    int k = it.PolynomialPosition();
    int s = it.MonomialSetOrder();
    const int* idx0 = it.multi_index();

    double factor = numi_.MonomialNaturalScales(c, s);
    Polynomial p0(d_, idx0, factor);
    p0.set_origin(xc);

    polys[0] = &p0;

    for (auto jt = it; jt.end() <= p.end(); ++jt) {
      const int* idx1 = jt.multi_index();
      int l = jt.PolynomialPosition();
      int t = jt.MonomialSetOrder();

      double factor = numi_.MonomialNaturalScales(c, t);
      Polynomial p1(d_, idx1, factor);
      p1.set_origin(xc);

      polys[1] = &p1;

      // sum up local contributions
      for (auto n = 0; n < nfaces; ++n) {
        int f = faces[n];
        mesh_->face_get_nodes(f, &nodes);
        mesh_->node_get_coordinates(nodes[0], &(xy[1]));
        mesh_->node_get_coordinates(nodes[1], &(xy[2]));

        polys[2] = &(K[n]);

        M(k, l) += numi_.IntegratePolynomialsTriangle(xy, polys);
      }
    }
  }

  // symmetric part of mass matrix
  for (int k = 0; k < nrows; ++k) {
    for (int l = k + 1; l < nrows; ++l) {
      M(l, k) = M(k, l); 
    }
  }

  basis_[c]->ChangeBasisMatrix(M);

  return 0;
}


/* ******************************************************************
* Stiffness matrix for Taylor basis functions. 
****************************************************************** */
int DG_Modal::StiffnessMatrix(int c, const Tensor& K, DenseMatrix& A)
{
  Tensor Ktmp(d_, 2);
  if (K.rank() == 2) {
    Ktmp = K;
  } else {
    Ktmp.MakeDiagonal(K(0, 0));
  }

  double scale = numi_.MonomialNaturalScales(c, 1);

  // extend list of integrals of monomials
  UpdateIntegrals_(c, 2 * order_ - 2);
  const Polynomial& integrals = integrals_[c];
   
  // copy integrals to mass matrix
  int multi_index[3];
  Polynomial p(d_, order_);

  int nrows = p.size();
  A.Reshape(nrows, nrows);

  for (auto it = p.begin(); it.end() <= p.end(); ++it) {
    const int* index = it.multi_index();
    int k = it.PolynomialPosition();

    for (auto jt = it; jt.end() <= p.end(); ++jt) {
      const int* jndex = jt.multi_index();
      int l = jt.PolynomialPosition();
      
      int n(0);
      int multi_index[3];
      for (int i = 0; i < d_; ++i) {
        multi_index[i] = index[i] + jndex[i];
        n += multi_index[i];
      }

      double sum(0.0), tmp;
      for (int i = 0; i < d_; ++i) {
        for (int j = 0; j < d_; ++j) {
          if (index[i] > 0 && jndex[j] > 0) {
            multi_index[i]--;
            multi_index[j]--;

            tmp = integrals(n - 2, p.MonomialSetPosition(multi_index)); 
            sum += Ktmp(i, j) * tmp * index[i] * jndex[j];

            multi_index[i]++;
            multi_index[j]++;
          }
        }
      }

      A(l, k) = A(k, l) = sum * scale * scale; 
    }
  }

  basis_[c]->ChangeBasisMatrix(A);

  return 0;
}


/* ******************************************************************
* Advection matrix for Taylor basis and cell-based velocity u.
****************************************************************** */
int DG_Modal::AdvectionMatrixPoly_(
    int c, const VectorPolynomial& u, DenseMatrix& A, bool grad_on_test)
{
  // rebase the polynomial
  const AmanziGeometry::Point& xc = mesh_->cell_centroid(c);

  VectorPolynomial ucopy(u);
  for (int i = 0; i < d_; ++i) {
    ucopy[i].ChangeOrigin(xc);
    numi_.ChangeBasisRegularToNatural(c, ucopy[i]);
  }

  // extend list of integrals of monomials
  int uk(ucopy[0].order());
  UpdateIntegrals_(c, order_ + std::max(0, order_ - 1) + uk);
  const Polynomial& integrals = integrals_[c];

  // gradient of a naturally scaled polynomial needs correction
  double scale = numi_.MonomialNaturalScales(c, 1);

  // sum-up integrals to the advection matrix
  int multi_index[3];
  Polynomial p(d_, order_), q(d_, order_);
  VectorPolynomial pgrad;

  int nrows = p.size();
  A.Reshape(nrows, nrows);
  A.PutScalar(0.0);

  for (auto it = p.begin(); it.end() <= p.end(); ++it) {
    const int* idx_p = it.multi_index();
    int k = it.PolynomialPosition();

    // product of polynomials need to align origins
    Polynomial pp(d_, idx_p, scale);
    pp.set_origin(xc);

    pgrad.Gradient(pp);
    Polynomial tmp(pgrad * ucopy);

    for (auto mt = tmp.begin(); mt.end() <= tmp.end(); ++mt) {
      const int* idx_K = mt.multi_index();
      int m = mt.MonomialSetPosition();
      double factor = tmp(mt.end(), m);
      if (factor == 0.0) continue;

      for (auto jt = q.begin(); jt.end() <= q.end(); ++jt) {
        const int* idx_q = jt.multi_index();
        int l = q.PolynomialPosition(idx_q);

        int n(0);
        for (int i = 0; i < d_; ++i) {
          multi_index[i] = idx_q[i] + idx_K[i];
          n += multi_index[i];
        }

        A(k, l) += factor * integrals(n, p.MonomialSetPosition(multi_index));
      }
    }
  }

  // gradient operator is applied to solution
  if (!grad_on_test) {
    A.Transpose();
  }

  basis_[c]->ChangeBasisMatrix(A);

  return 0;
}


/* ******************************************************************
* Advection matrix for Taylor basis and piecewise polynomial velocity.
****************************************************************** */
int DG_Modal::AdvectionMatrixPiecewisePoly_(
    int c, const VectorPolynomial& u, DenseMatrix& A, bool grad_on_test)
{
  AmanziMesh::Entity_ID_List faces, nodes;
  mesh_->cell_get_faces(c, &faces);
  int nfaces = faces.size();

  const AmanziGeometry::Point& xc = mesh_->cell_centroid(c);

  // rebase the velocity polynomial (due to dot-product)
  VectorPolynomial ucopy(u);
  for (int i = 0; i < u.size(); ++i) {
    ucopy[i].ChangeOrigin(xc);
  }

  // allocate memory for matrix
  Polynomial p(d_, order_), q(d_, order_);
  VectorPolynomial pgrad;

  int nrows = p.size();
  A.Reshape(nrows, nrows);
  A.PutScalar(0.0);

  std::vector<const Polynomial*> polys(2);

  std::vector<AmanziGeometry::Point> xy(3); 
  // xy[0] = cell_geometric_center(*mesh_, c);
  xy[0] = mesh_->cell_centroid(c);

  for (auto it = p.begin(); it.end() <= p.end(); ++it) {
    int k = it.PolynomialPosition();
    int s = it.MonomialSetOrder();
    const int* idx0 = it.multi_index();

    double factor = numi_.MonomialNaturalScales(c, s);
    Polynomial p0(d_, idx0, factor);
    p0.set_origin(xc);

    pgrad.Gradient(p0);

    for (auto jt = q.begin(); jt.end() <= q.end(); ++jt) {
      const int* idx1 = jt.multi_index();
      int l = jt.PolynomialPosition();
      int t = jt.MonomialSetOrder();

      double factor = numi_.MonomialNaturalScales(c, t);
      Polynomial p1(d_, idx1, factor);
      p1.set_origin(xc);

      polys[0] = &p1;

      // sum-up integrals to the advection matrix
      for (int n = 0; n < nfaces; ++n) {
        int f = faces[n];
        mesh_->face_get_nodes(f, &nodes);
        mesh_->node_get_coordinates(nodes[0], &(xy[1]));
        mesh_->node_get_coordinates(nodes[1], &(xy[2]));

        Polynomial tmp(d_, 0);
        tmp.set_origin(xc);
        for (int i = 0; i < d_; ++i) {
          tmp += pgrad[i] * ucopy[n * d_ + i];
        }
        polys[1] = &tmp;

        A(k, l) += numi_.IntegratePolynomialsTriangle(xy, polys);
      }
    }
  }

  // gradient operator is applied to solution
  if (!grad_on_test) {
    A.Transpose();
  }

  basis_[c]->ChangeBasisMatrix(A);

  return 0;
}


/* ******************************************************************
* Upwind/Downwind matrix for Taylor basis and normal velocity u.n. 
* If jump_on_test=true, we calculate
* 
*   \Int { (u.n) \rho^* [\psi] } dS
* 
* where star means downwind, \psi is a test function, and \rho is 
* a solution. Otherwise, we calculate 
*
*   \Int { (u.n) \psi^* [\rho] } dS
****************************************************************** */
int DG_Modal::FluxMatrix(int f, const Polynomial& un, DenseMatrix& A,
                         bool upwind, bool jump_on_test)
{
  AmanziMesh::Entity_ID_List cells, nodes;
  mesh_->face_get_cells(f, AmanziMesh::Parallel_type::ALL, &cells);
  int ncells = cells.size();

  Polynomial poly0(d_, order_), poly1(d_, order_);
  int size = poly0.size();

  int nrows = ncells * size;
  A.Reshape(nrows, nrows);
  A.PutScalar(0.0);

  // identify index of upwind/downwind cell (id) 
  int dir, id(0); 
  mesh_->face_normal(f, false, cells[0], &dir);
  const AmanziGeometry::Point& xf = mesh_->face_centroid(f);

  if (ncells > 1) {
    double vel = un.Value(xf) * dir;
    if (upwind) vel = -vel;

    if (vel > 0.0) {
      id = 1;
    } else {
      dir = -dir;
    }
  }

  int col(id * size);
  int row(size - col);

  // Calculate integrals needed for scaling
  int c1, c2;
  c1 = cells[id];
  UpdateIntegrals_(c1, 2 * order_);

  if (ncells == 1) {
    c2 = c1;
  } else {
    c2 = cells[1 - id];
    UpdateIntegrals_(c2, 2 * order_);
  }

  // integrate traces of polynomials on face f
  std::vector<const Polynomial*> polys(3);

  for (auto it = poly0.begin(); it.end() <= poly0.end(); ++it) {
    const int* idx0 = it.multi_index();
    int k = poly0.PolynomialPosition(idx0);
    int s = it.MonomialSetOrder();

    double factor = numi_.MonomialNaturalScales(c1, s);
    Polynomial p0(d_, idx0, factor);
    p0.set_origin(mesh_->cell_centroid(c1));

    factor = numi_.MonomialNaturalScales(c2, s);
    Polynomial p1(d_, idx0, factor);
    p1.set_origin(mesh_->cell_centroid(c2));

    for (auto jt = poly1.begin(); jt.end() <= poly1.end(); ++jt) {
      const int* idx1 = jt.multi_index();
      int l = poly1.PolynomialPosition(idx1);
      int t = jt.MonomialSetOrder();

      factor = numi_.MonomialNaturalScales(c1, t);
      Polynomial q(d_, idx1, factor);
      q.set_origin(mesh_->cell_centroid(c1));

      polys[0] = &un;
      polys[1] = &p0;
      polys[2] = &q;

      // downwind-downwind integral
      double vel1 = numi_.IntegratePolynomialsFace(f, polys);
      vel1 /= mesh_->face_area(f);
      vel1 *= dir;  

      // upwind-downwind integral
      polys[1] = &p1;

      double vel0 = numi_.IntegratePolynomialsFace(f, polys);
      vel0 /= mesh_->face_area(f);
      vel0 *= dir;  

      if (ncells == 1) {
        A(k, l) = vel1;
      } else {
        A(row + k, col + l) = vel0;
        A(col + k, col + l) = -vel1;
      }
    }
  }

  // jump operator is applied to solution
  if (!jump_on_test) {
    A.Transpose();
  }

  if (ncells == 1) {
    basis_[cells[0]]->ChangeBasisMatrix(A);
  } else { 
    basis_[cells[0]]->ChangeBasisMatrix(basis_[cells[0]], basis_[cells[1]], A);
  }

  return 0;
}


/* ******************************************************************
* Rusanov flux matrix for Taylor basis and normal velocity u.n. 
* Velocities are given in the face-based Taylor basis. We calculate
* 
*   \Int { (u.n \rho)^* [\psi] } dS
*
* where (u.n \rho)^* is the Rusanov flux.
****************************************************************** */
int DG_Modal::FluxMatrixRusanov(
    int f, const VectorPolynomial& uc1, const VectorPolynomial& uc2,
    const Polynomial& uf, DenseMatrix& A)
{
  AmanziMesh::Entity_ID_List cells, nodes;
  mesh_->face_get_cells(f, Parallel_type::ALL, &cells);
  int ncells = cells.size();

  Polynomial poly0(d_, order_), poly1(d_, order_);
  int size = poly0.size();

  int nrows = ncells * size;
  A.Reshape(nrows, nrows);
  A.PutScalar(0.0);
  if (ncells == 1) return 0;  // FIXME

  // identify index of downwind cell (id)
  int dir; 
  AmanziGeometry::Point normal = mesh_->face_normal(f, false, cells[0], &dir);
  const AmanziGeometry::Point& xf = mesh_->face_centroid(f);

  // Calculate integrals needed for scaling
  int c1 = cells[0];
  int c2 = cells[1];

  UpdateIntegrals_(c1, 2 * order_);
  UpdateIntegrals_(c2, 2 * order_);

  // integrate traces of polynomials on face f
  normal *= -1;
  Polynomial uf1 = uc1 * normal;
  Polynomial uf2 = uc2 * normal;

  uf2.ChangeOrigin(uf1.origin());
  Polynomial ufn = (uf1 + uf2) * 0.5;

  double tmp = numi_.PolynomialMaxValue(f, ufn);
  tmp *= 0.5;
  uf1(0, 0) -= tmp;
  uf2(0, 0) += tmp;

  std::vector<const Polynomial*> polys(3);

  for (auto it = poly0.begin(); it.end() <= poly0.end(); ++it) {
    const int* idx0 = it.multi_index();
    int k = poly0.PolynomialPosition(idx0);
    int s = it.MonomialSetOrder();

    double factor = numi_.MonomialNaturalScales(c1, s);
    Polynomial p0(d_, idx0, factor);
    p0.set_origin(mesh_->cell_centroid(c1));

    factor = numi_.MonomialNaturalScales(c2, s);
    Polynomial p1(d_, idx0, factor);
    p1.set_origin(mesh_->cell_centroid(c2));

    for (auto jt = poly1.begin(); jt.end() <= poly1.end(); ++jt) {
      const int* idx1 = jt.multi_index();
      int l = poly1.PolynomialPosition(idx1);
      int t = jt.MonomialSetOrder();

      factor = numi_.MonomialNaturalScales(c1, t);
      Polynomial q0(d_, idx1, factor);
      q0.set_origin(mesh_->cell_centroid(c1));

      factor = numi_.MonomialNaturalScales(c2, t);
      Polynomial q1(d_, idx1, factor);
      q1.set_origin(mesh_->cell_centroid(c2));

      double coef00, coef01, coef10, coef11;
      double scale = 2 * mesh_->face_area(f);

      // upwind-upwind integral
      polys[0] = &uf1;
      polys[1] = &p0;
      polys[2] = &q0;
      coef00 = numi_.IntegratePolynomialsFace(f, polys);

      // upwind-downwind integral
      polys[2] = &q1;
      coef01 = numi_.IntegratePolynomialsFace(f, polys);

      // downwind-downwind integral
      polys[0] = &uf2;
      polys[1] = &p1;
      coef11 = numi_.IntegratePolynomialsFace(f, polys);

      // downwind-upwind integral
      polys[2] = &q0;
      coef10 = numi_.IntegratePolynomialsFace(f, polys);

      A(l, k) = coef00 / scale;
      A(size + l, k) = -coef01 / scale;
      A(l, size + k) = coef10 / scale;
      A(size + l, size + k) = -coef11 / scale;
    }
  }

  basis_[cells[0]]->ChangeBasisMatrix(basis_[cells[0]], basis_[cells[1]], A);

  return 0;
}


/* *****************************************************************
* Jump matrix for Taylor basis:
*
*   \Int_f ( {K \grad \rho} [\psi] ) dS
****************************************************************** */
int DG_Modal::FaceMatrixJump(int f, const Tensor& K1, const Tensor& K2, DenseMatrix& A)
{
  AmanziMesh::Entity_ID_List cells;
  mesh_->face_get_cells(f, Parallel_type::ALL, &cells);
  int ncells = cells.size();

  Polynomial poly0(d_, order_), poly1(d_, order_);
  int size = poly0.size();

  int nrows = ncells * size;
  A.Reshape(nrows, nrows);

  // Calculate integrals needed for scaling
  int c1 = cells[0];
  int c2 = (ncells > 1) ? cells[1] : -1;

  UpdateIntegrals_(c1, 2 * order_ - 1);
  if (c2 >= 0) {
    UpdateIntegrals_(c2, 2 * order_ - 1);
  }

  // Calculate co-normals
  int dir;
  AmanziGeometry::Point conormal1(d_), conormal2(d_);
  AmanziGeometry::Point normal = mesh_->face_normal(f, false, c1, &dir);

  normal /= norm(normal);
  conormal1 = K1 * normal;
  if (c2 >= 0) {
    conormal2 = K2 * normal;
  }

  // integrate traces of polynomials on face f
  double coef00, coef01, coef10, coef11;
  Polynomial p0, p1, q0, q1;
  VectorPolynomial pgrad;
  std::vector<const Polynomial*> polys(2);

  for (auto it = poly0.begin(); it.end() <= poly0.end(); ++it) {
    const int* idx0 = it.multi_index();
    int k = poly0.PolynomialPosition(idx0);
    int s = it.MonomialSetOrder();

    double factor = numi_.MonomialNaturalScales(c1, s);
    Polynomial p0(d_, idx0, factor);
    p0.set_origin(mesh_->cell_centroid(c1));

    pgrad.Gradient(p0);
    p0 = pgrad * conormal1;

    for (auto jt = poly1.begin(); jt.end() <= poly1.end(); ++jt) {
      const int* idx1 = jt.multi_index();
      int l = poly1.PolynomialPosition(idx1);
      int t = jt.MonomialSetOrder();

      factor = numi_.MonomialNaturalScales(c1, t);
      Polynomial q0(d_, idx1, factor);
      q0.set_origin(mesh_->cell_centroid(c1));

      polys[0] = &p0;
      polys[1] = &q0;
      coef00 = numi_.IntegratePolynomialsFace(f, polys);

      A(k, l) = coef00 / ncells;

      if (c2 >= 0) {
        factor = numi_.MonomialNaturalScales(c2, s);
        Polynomial p1(d_, idx0, factor);
        p1.set_origin(mesh_->cell_centroid(c2));

        pgrad.Gradient(p1);
        p1 = pgrad * conormal2;

        factor = numi_.MonomialNaturalScales(c2, t);
        Polynomial q1(d_, idx1, factor);
        q1.set_origin(mesh_->cell_centroid(c2));

        polys[1] = &q1;
        coef01 = numi_.IntegratePolynomialsFace(f, polys);

        polys[0] = &p1;
        coef11 = numi_.IntegratePolynomialsFace(f, polys);

        polys[1] = &q0;
        coef10 = numi_.IntegratePolynomialsFace(f, polys);

        A(k, size + l) = -coef01 / ncells;
        A(size + k, size + l) = -coef11 / ncells;
        A(size + k, l) = coef10 / ncells;
      }
    }
  }

  if (ncells == 1) {
    basis_[c1]->ChangeBasisMatrix(A);
  } else {
    basis_[c1]->ChangeBasisMatrix(basis_[c1], basis_[c2], A);
  }

  return 0;
}


/* *****************************************************************
* Penalty matrix for Taylor basis and penalty coefficient Kf
* corresponding to the following integral:
*
*   \Int_f { K_f [\psi] [\rho] } dS
****************************************************************** */
int DG_Modal::FaceMatrixPenalty(int f, double Kf, DenseMatrix& A)
{
  AmanziMesh::Entity_ID_List cells;
  mesh_->face_get_cells(f, Parallel_type::ALL, &cells);
  int ncells = cells.size();

  Polynomial poly0(d_, order_), poly1(d_, order_);
  int size = poly0.size();

  int nrows = ncells * size;
  A.Reshape(nrows, nrows);

  // Calculate integrals needed for scaling
  int c1 = cells[0];
  int c2 = (ncells > 1) ? cells[1] : -1;

  UpdateIntegrals_(c1, 2 * order_);
  if (c2 >= 0) {
    UpdateIntegrals_(c2, 2 * order_);
  }

  // integrate traces of polynomials on face f
  double coef00, coef01, coef11;
  Polynomial p0, p1, q0, q1;
  std::vector<const Polynomial*> polys(2);

  for (auto it = poly0.begin(); it.end() <= poly0.end(); ++it) {
    const int* idx0 = it.multi_index();
    int k = poly0.PolynomialPosition(idx0);
    int s = it.MonomialSetOrder();

    double factor = numi_.MonomialNaturalScales(c1, s);
    Polynomial p0(d_, idx0, factor);
    p0.set_origin(mesh_->cell_centroid(c1));

    for (auto jt = poly1.begin(); jt.end() <= poly1.end(); ++jt) {
      const int* idx1 = jt.multi_index();
      int l = poly1.PolynomialPosition(idx1);
      int t = jt.MonomialSetOrder();

      factor = numi_.MonomialNaturalScales(c1, t);
      Polynomial q0(d_, idx1, factor);
      q0.set_origin(mesh_->cell_centroid(c1));

      polys[0] = &p0;
      polys[1] = &q0;
      coef00 = numi_.IntegratePolynomialsFace(f, polys);

      A(k, l) = Kf * coef00;

      if (c2 >= 0) {
        factor = numi_.MonomialNaturalScales(c2, s);
        Polynomial p1(d_, idx0, factor);
        p1.set_origin(mesh_->cell_centroid(c2));

        factor = numi_.MonomialNaturalScales(c2, t);
        Polynomial q1(d_, idx1, factor);
        q1.set_origin(mesh_->cell_centroid(c2));

        polys[1] = &q1;
        coef01 = numi_.IntegratePolynomialsFace(f, polys);

        polys[0] = &p1;
        coef11 = numi_.IntegratePolynomialsFace(f, polys);

        A(k, size + l) = -Kf * coef01;
        A(size + k, size + l) = Kf * coef11;
        A(size + l, k) = -Kf * coef01;
      }
    }
  }

  if (ncells == 1) {
    basis_[c1]->ChangeBasisMatrix(A);
  } else {
    basis_[c1]->ChangeBasisMatrix(basis_[c1], basis_[c2], A);
  }

  return 0;
}


/* ******************************************************************
* Update integrals of non-normalized monomials.
****************************************************************** */
void DG_Modal::UpdateIntegrals_(int c, int order)
{
  if (integrals_.size() == 0) {
    int ncells_wghost = mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::Parallel_type::ALL);
    integrals_.resize(ncells_wghost);

    for (int n = 0; n < ncells_wghost; ++n) {
      integrals_[n].Reshape(d_, 0);
      integrals_[n](0, 0) = mesh_->cell_volume(n);
    }
  }

  // add additional integrals of monomials
  int k0 = integrals_[c].order();
  if (k0 < order) {
    integrals_[c].Reshape(d_, order);

    for (int k = k0 + 1; k <= order; ++k) {
      numi_.IntegrateMonomialsCell(c, k, integrals_[c]);
    }
  }
}


/* ******************************************************************
* Normalize and optionally orthogonalize Taylor basis functions.
****************************************************************** */
void DG_Modal::UpdateScales_(int c, int order)
{
  if (scales_a_.size() == 0) {
    int ncells_wghost = mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::Parallel_type::ALL);
    scales_a_.resize(ncells_wghost);
    scales_b_.resize(ncells_wghost);

    for (int n = 0; n < ncells_wghost; ++n) {
      scales_a_[n].Reshape(d_, order);
      scales_b_[n].Reshape(d_, order);
    }

    // For the moment, we update everything in one shot
    for (int n = 0; n < ncells_wghost; ++n) {
      UpdateIntegrals_(n, 2 * order);

      const Polynomial& integrals = integrals_[n];
      Polynomial poly(d_, order);

      double a, b, norm;
      double volume = integrals(0, 0); 

      for (auto it = poly.begin(); it.end() <= poly.end(); ++it) {
        int k = it.MonomialSetPosition();
        const int* multi_index = it.multi_index();
        int index[d_]; 

        int m(0);
        for (int i = 0; i < d_; ++i) {
          m += multi_index[i];
          index[i] = 2 * multi_index[i];
        }

        if (m == 0) {
          a = 1.0;
          b = 0.0;
        } else {
          if (basis_[0]->id() == TAYLOR_BASIS_NORMALIZED_ORTHO) {
            b = integrals(m, k) / volume;
          } else {
            b = 0.0;  // no orthogonalization to constants
          }

          norm = integrals(2 * m, integrals.MonomialSetPosition(index));
          norm -= b * b * volume;

          a = std::pow(volume / norm, 0.5);
        }

        scales_a_[n](m, k) = a;
        scales_b_[n](m, k) = b;
      }
    }
  }
}

}  // namespace WhetStone
}  // namespace Amanzi

