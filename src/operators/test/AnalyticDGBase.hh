/*
  Operators

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Base class for testing DG schemes for diffusion and advection
  problems of the form:

    a du/dt + v . grad(u) - div(K grad(u)) + r u = f.

  List of solutions:
    AnalyticDG0n: polynomial solution of order n, where n=0,1,2,3.
    AnalyticDG04: sin(3x) six(6y)
    AnalyticDG06: level-set circle problem with divergence-free velocity
*/

#ifndef AMANZI_OPERATOR_ANALYTIC_DG_BASE_HH_
#define AMANZI_OPERATOR_ANALYTIC_DG_BASE_HH_

#include "Teuchos_RCP.hpp"

#include "DG_Modal.hh"
#include "Mesh.hh"
#include "Point.hh"
#include "Polynomial.hh"
#include "VectorPolynomial.hh"
#include "Tensor.hh"

class AnalyticDGBase {
 public:
  AnalyticDGBase(Teuchos::RCP<const Amanzi::AmanziMesh::Mesh> mesh, int order)
    : mesh_(mesh),
      order_(order),
      d_(mesh_->space_dimension()) {};
  ~AnalyticDGBase() {};

  // analytic data in conventional Taylor basis
  // -- diffusion tensor
  virtual Amanzi::WhetStone::Tensor Tensor(const Amanzi::AmanziGeometry::Point& p, double t) = 0;

  // -- solution
  virtual void SolutionTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                              Amanzi::WhetStone::Polynomial& coefs) = 0;

  // -- velocity
  virtual void VelocityTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                              Amanzi::WhetStone::VectorPolynomial& v) = 0;

  // -- accumulation
  virtual void AccumulationTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                                  Amanzi::WhetStone::Polynomial& a) = 0;

  // -- reaction
  virtual void ReactionTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                              Amanzi::WhetStone::Polynomial& r) = 0;

  // -- source term
  virtual void SourceTaylor(const Amanzi::AmanziGeometry::Point& p, double t,
                            Amanzi::WhetStone::Polynomial& src) = 0;

  // exact pointwise values
  // -- solution
  double SolutionExact(const Amanzi::AmanziGeometry::Point& p, double t) {
    Amanzi::WhetStone::Polynomial coefs;
    SolutionTaylor(p, t, coefs);
    return coefs(0, 0);
  }

  // -- velocity
  virtual Amanzi::AmanziGeometry::Point VelocityExact(const Amanzi::AmanziGeometry::Point& p, double t) {
    Amanzi::WhetStone::VectorPolynomial v;
    VelocityTaylor(p, t, v);

    Amanzi::AmanziGeometry::Point tmp(d_);
    for (int i = 0; i < d_; ++i) {
      tmp[i] = v[i](0, 0);
    }
    return tmp;
  }

  // initial guess
  void InitialGuess(Amanzi::WhetStone::DG_Modal& dg, Epetra_MultiVector& p, double t) {
    Amanzi::WhetStone::Polynomial coefs;
    Amanzi::WhetStone::NumericalIntegration numi(mesh_, false);

    int ncells = mesh_->num_entities(Amanzi::AmanziMesh::CELL, Amanzi::AmanziMesh::Parallel_type::ALL);
    for (int c = 0; c < ncells; ++c) {
      const Amanzi::AmanziGeometry::Point& xc = mesh_->cell_centroid(c);
      SolutionTaylor(xc, t, coefs);
      numi.ChangeBasisRegularToNatural(c, coefs);

      Amanzi::WhetStone::DenseVector data;
      coefs.GetPolynomialCoefficients(data);

      const Amanzi::WhetStone::Basis& basis = dg.cell_basis(c);
      basis.ChangeBasisVector(data);

      for (int n = 0; n < data.NumRows(); ++n) {
        p[n][c] = data(n);
      }
    }
  }

  // error calculations
  void ComputeCellError(Epetra_MultiVector& p, double t, 
                        double& pnorm, double& l2_err, double& inf_err,
                                       double& l2_mean, double& inf_mean) {
    pnorm = 0.0;
    l2_err = l2_mean = 0.0;
    inf_err = inf_mean = 0.0;

    Amanzi::WhetStone::NumericalIntegration numi(mesh_, false);

    int ncells = mesh_->num_entities(Amanzi::AmanziMesh::CELL, Amanzi::AmanziMesh::Parallel_type::OWNED);
    for (int c = 0; c < ncells; c++) {
      const Amanzi::AmanziGeometry::Point& xc = mesh_->cell_centroid(c);
      double volume = mesh_->cell_volume(c);

      int nk = p.NumVectors();
      Amanzi::WhetStone::DenseVector data(nk);
      Amanzi::WhetStone::Polynomial sol, poly(d_, order_); 

      for (int i = 0; i < nk; ++i) data(i) = p[i][c];
      poly.SetPolynomialCoefficients(data);
      poly.set_origin(xc);

      SolutionTaylor(xc, t, sol);
      numi.ChangeBasisRegularToNatural(c, sol);

      Amanzi::WhetStone::Polynomial poly_err(poly);
      poly_err -= sol;
      double err = poly_err.NormMax();

      l2_err += err * err * volume;
      inf_err = std::max(inf_err, fabs(err));

      err = poly_err(0, 0);
      l2_mean += err * err * volume;
      inf_mean = std::max(inf_mean, fabs(err));
      // std::cout << c << " Exact: " << sol << "dG:" << poly << "ERRs: " << l2_err << " " << inf_err << "\n\n";

      pnorm += std::pow(sol(0, 0), 2.0) * volume;
    }
#ifdef HAVE_MPI
    double tmp_out[3], tmp_ina[3] = {pnorm, l2_err, l2_mean};
    mesh_->get_comm()->SumAll(tmp_ina, tmp_out, 3);
    pnorm = tmp_out[0];
    l2_err = tmp_out[1];
    l2_mean = tmp_out[2];

    double tmp_inb[2] = {inf_err, inf_mean};
    mesh_->get_comm()->MaxAll(tmp_inb, tmp_out, 2);
    inf_err = tmp_out[0];
    inf_mean = tmp_out[1];
#endif
    pnorm = sqrt(pnorm);
    l2_err = sqrt(l2_err);
    l2_mean = sqrt(l2_mean);
  }

 protected:
  Teuchos::RCP<const Amanzi::AmanziMesh::Mesh> mesh_;
  int order_, d_;
};

#endif

