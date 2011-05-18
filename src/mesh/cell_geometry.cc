#include "cell_geometry.hh"
#include "Cell_topology.hh"
#include <math.h>

namespace cell_geometry {

  double dot_product(const double a[], const double b[], int n)
  {
    double dp = 0.0;
    for (int i = 0; i < n; ++i)
      dp += a[i]*b[i];
    return dp;
  }


  double vector_length(const double x[], int n)
  {
    double a, b, c, t;
    switch (n) {
      case 2:  // two-vector
        a = fabs(x[0]);
        b = fabs(x[1]);
        // Swap largest value to A.
        if (b > a) {
          t = a;
          a = b;
          b = t;
        }
        return a * sqrt(1.0 + (b/a)*(b/a));

      case 3:  // three-vector
        a = fabs(x[0]);
        b = fabs(x[1]);
        c = fabs(x[2]);
        // Swap largest value to A.
        if (b > a) {
          if (c > b) {
            t = a;
            a = c;
            c = t;
          } else {
            t = a;
            a = b;
            b = t;
          }
        } else if (c > a) {
          t = a;
          a = c;
          c = t;
        }
        return a * sqrt(1.0 + (b/a)*(b/a) + (c/a)*(c/a));

      default:  // Do the na�ve thing for anything else.
        a = 0.0;
        for (int i = 0; i < n; ++i)
          a += x[i]*x[i];
        return sqrt(a);
    }
  }


  void cross_product(const double a[], const double b[], double result[])
  {
    result[0] = a[1]*b[2] - a[2]*b[1];
    result[1] = a[2]*b[0] - a[0]*b[2];
    result[2] = a[0]*b[1] - a[1]*b[0];
  };


  double triple_product(const double a[], const double b[], const double c[])
  {
    return a[0]*(b[1]*c[2] - b[2]*c[1]) +
           a[1]*(b[2]*c[0] - b[0]*c[2]) + 
           a[2]*(b[0]*c[1] - b[1]*c[0]);
  };
  

  void quad_face_normal(const double x1[], const double x2[], const double x3[], const double x4[], double result[])
  {
    double v1[3], v2[3];
    for (int i = 0; i < 3; ++i) {
      v1[i] = x3[i] - x1[i];
      v2[i] = x4[i] - x2[i];
    }
    cross_product(v1, v2, result);
    for (int i = 0; i < 3; ++i)
      result[i] = 0.5 * result[i];
  }

  void quad_face_normal(const double x[][3], double result[])
  { quad_face_normal(x[0], x[1], x[2], x[3], result); }
  
  void quad_face_normal(const Epetra_SerialDenseMatrix &x, double result[])
  { quad_face_normal(x[0], x[1], x[2], x[3], result); }

  void quad_face_normal(double *x, double result[])
  {
    Epetra_SerialDenseMatrix X(View, x, 3, 3, 4);
    quad_face_normal(X, result);
  }


  double quad_face_area(const double x1[], const double x2[], const double x3[], const double x4[])
  {
    double a[3];
    quad_face_normal(x1, x2, x3, x4, a);
    return vector_length(a,3);
  }

  
  double tet_volume(const double x1[], const double x2[], const double x3[], const double x4[])
  {
    double v1[3], v2[3], v3[3];

    for (int i = 0; i < 3; ++i) {
      v1[i] = x2[i] - x1[i];
      v2[i] = x3[i] - x1[i];
      v3[i] = x4[i] - x1[i];
    }
    return triple_product(v1, v2, v3) / 6.0;
  }
  
  
  double hex_volume(const Epetra_SerialDenseMatrix &x)
  {
    double hvol, cvol[8];
    compute_hex_volumes(x, hvol, cvol);
    return hvol;
  }
  
  
  void compute_hex_volumes (const Epetra_SerialDenseMatrix &x, double &hvol, double cvol[])
  {
    cvol[0] = tet_volume(x[0], x[1], x[3], x[4]);
    cvol[1] = tet_volume(x[1], x[2], x[0], x[5]); 
    cvol[2] = tet_volume(x[2], x[3], x[1], x[6]); 
    cvol[3] = tet_volume(x[3], x[0], x[2], x[7]); 
    cvol[4] = tet_volume(x[4], x[7], x[5], x[0]); 
    cvol[5] = tet_volume(x[5], x[4], x[6], x[1]);
    cvol[6] = tet_volume(x[6], x[5], x[7], x[2]);
    cvol[7] = tet_volume(x[7], x[6], x[4], x[3]); 

    hvol = 0.0;
    for (int i = 0; i < 8; ++i) hvol += cvol[i];

    hvol += tet_volume(x[0],x[2],x[7],x[5]) + tet_volume(x[1],x[3],x[4],x[6]);
    hvol *= 0.5;
  }


  void compute_hex_face_normals(const Epetra_SerialDenseMatrix &x, Epetra_SerialDenseMatrix &a)
  {
    double v1[3], v2[3];

    quad_face_normal(x[0], x[1], x[5], x[4], a[0]);
    quad_face_normal(x[1], x[2], x[6], x[5], a[1]);
    quad_face_normal(x[2], x[3], x[7], x[6], a[2]);
    quad_face_normal(x[3], x[0], x[4], x[7], a[3]);
    quad_face_normal(x[0], x[3], x[2], x[1], a[4]);
    quad_face_normal(x[4], x[5], x[6], x[7], a[5]);
  }
  
  
  // Correct for hexes with planar faces (at least).
  void hex_centroid (const Epetra_SerialDenseMatrix &x, double c[])
  {
    double hvol = 0.0;
    for (int i = 0; i < 3; ++i) c[i] = 0.0;
    
    for (int j = 0; j < 10; ++j) {
      const int *tvert = Amanzi::AmanziMesh::HexTetVert[j];
      double tvol = tet_volume(x[tvert[0]], x[tvert[1]], x[tvert[2]], x[tvert[3]]);
      hvol += tvol;
      for (int i = 0; i < 3; ++i) {
        double s = 0.0;
        for (int k = 0; k < 4; ++k) s += x[tvert[k]][i];
        c[i] += tvol * s;
      }
    }
    
    hvol = 0.5 * hvol;
    for (int i = 0; i < 3; ++i) c[i] = c[i] / (8 * hvol);
  }
  
  // Correct only for planar faces.
  void quad_face_centroid(const Epetra_SerialDenseMatrix &x, double c[])
  {
    double a0 = tri_face_area(x[0], x[1], x[3]);
    double a2 = tri_face_area(x[2], x[3], x[1]);
    
    double c0[3], c2[3];
    tri_face_centroid(x[0], x[1], x[3], c0);
    tri_face_centroid(x[2], x[3], x[1], c2);
    
    for (int i = 0; i < 3; ++i) c[i] = (a0*c0[i] + a2*c2[i]) / (a0 + a2);
  }
  
  double tri_face_area(const double x0[], const double x1[], const double x2[])
  {
    double v1[3], v2[3];
    for (int i = 0; i < 3; ++i) {
      v1[i] = x1[i] - x0[i];
      v2[i] = x2[i] - x0[i];
    }
    double a[3];
    cross_product(v1, v2, a);
    return (0.5 * vector_length(a, 3));
  }
  
  void tri_face_centroid(const double x0[], const double x1[], const double x2[], double c[])
  {
    for (int i = 0; i < 3; ++i) c[i] = (x0[i] + x1[i] + x2[i]) / 3.0;
  }

}
