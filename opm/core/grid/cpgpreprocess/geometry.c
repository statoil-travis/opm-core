/*
 * Copyright 2010 (c) SINTEF ICT, Applied Mathematics.
 * Jostein R. Natvig <Jostein.R.Natvig at sintef.no>
 */
#include <math.h>
#include <stdio.h>
#include "geometry.h"
#include <assert.h>

/* ------------------------------------------------------------------ */
static void
cross(const double u[3], const double v[3], double w[3])
/* ------------------------------------------------------------------ */
{
   w[0] = u[1]*v[2]-u[2]*v[1];
   w[1] = u[2]*v[0]-u[0]*v[2];
   w[2] = u[0]*v[1]-u[1]*v[0];
}

/* ------------------------------------------------------------------ */
static double
norm(const double w[3])
/* ------------------------------------------------------------------ */
{
   return sqrt(w[0]*w[0] + w[1]*w[1] + w[2]*w[2]);
}


/* ------------------------------------------------------------------ */
static void
compute_face_geometry_3d(double *coords, int nfaces,
                         int *nodepos, int *facenodes, double *fnormals,
                         double *fcentroids, double *fareas)
/* ------------------------------------------------------------------ */
{

   /* Assume 3D for now */
   const int ndims = 3;
   int f;
   double x[3];
   double u[3];
   double v[3];
   double w[3];

   int i,k;
   int node;

   double cface[3]  = {0};
   double n[3]  = {0};
   const double twothirds = 0.666666666666666666666666666667;
   for (f=0; f<nfaces; ++f)
   {
      int    num_face_nodes;
      double area = 0.0;
      for(i=0; i<ndims; ++i) x[i] = 0.0;
      for(i=0; i<ndims; ++i) n[i] = 0.0;
      for(i=0; i<ndims; ++i) cface[i] = 0.0;

      /* average node */
      for(k=nodepos[f]; k<nodepos[f+1]; ++k)
      {
         node = facenodes[k];
         for (i=0; i<ndims; ++i) x[i] += coords[3*node+i];
      }
      num_face_nodes = nodepos[f+1] - nodepos[f];
      for(i=0; i<ndims; ++i) x[i] /= num_face_nodes;



      /* compute first vector u (to the last node in the face) */
      node = facenodes[nodepos[f+1]-1];
      for(i=0; i<ndims; ++i) u[i] = coords[3*node+i] - x[i];


      /* Compute triangular contrib. to face normal and face centroid*/
      for(k=nodepos[f]; k<nodepos[f+1]; ++k)
      {
         double a;

         node = facenodes[k];
         for (i=0; i<ndims; ++i) v[i] = coords[3*node+i] - x[i];

         cross(u,v,w);
         a = 0.5*norm(w);
         area += a;
         if(!(a>0))
         {
            fprintf(stderr, "Internal error in compute_face_geometry.");
         }

         /* face normal */
         for (i=0; i<ndims; ++i) n[i] += w[i];

         /* face centroid */
         for (i=0; i<ndims; ++i)
            cface[i] += a*(x[i]+twothirds*0.5*(u[i]+v[i]));

         /* Store v in u for next iteration */
         for (i=0; i<ndims; ++i) u[i] = v[i];
      }

      /* Store face normal and face centroid */
      for (i=0; i<ndims; ++i)
      {
         /* normal is scaled with face area */
         fnormals  [3*f+i] = 0.5*n[i];
         fcentroids[3*f+i] = cface[i]/area;
      }
      fareas[f] = area;
   }
}

/* ------------------------------------------------------------------ */
static void
compute_edge_geometry_2d(
      /* in  */ double *node_coords,
      /* in  */ int     num_edges,
      /* in  */ int    *edge_node_pos,
      /* in  */ int    *edge_nodes,
      /* out */ double *edge_normals,
      /* out */ double *edge_midpoints,
      /* out */ double *edge_lengths)
{
   const int num_dims = 2;

   /* offsets to each of the nodes in a compacted edge */
   const int a_ofs = 0;
   const int b_ofs = 1;

   /* offsets to each dimension is a compacted point */
   const int x_ofs = 0;
   const int y_ofs = 1;

   int edge;                     /* edge index       */
   int a_nod, b_nod;             /* node indices     */
   double a_x, a_y, b_x, b_y;    /* node coordinates */
   double v_x, v_y;              /* vector elements  */

   /* decompose each edge into a tuple (a,b) between two points and
    * compute properties for that face. hopefully the host has enough
    * cache pages to keep both input and output at the same time, and
    * registers for all the local variables */
   for (edge = 0; edge < num_edges; ++edge)
   {
      /* an edge in 2D can only have starting and ending point
       * check that there are exactly two nodes till the next edge */
      assert (edge_node_pos[edge + 1] - edge_node_pos[edge] == num_dims);

      /* get the first and last point on the edge */
      a_nod = edge_nodes[edge_node_pos[edge] + a_ofs];
      b_nod = edge_nodes[edge_node_pos[edge] + b_ofs];

      /* extract individual coordinates for the points */
      a_x = node_coords[a_nod * num_dims + x_ofs];
      a_y = node_coords[a_nod * num_dims + y_ofs];
      b_x = node_coords[b_nod * num_dims + x_ofs];
      b_y = node_coords[b_nod * num_dims + y_ofs];

      /* compute edge center -- average of node coordinates */
      edge_midpoints[edge * num_dims + x_ofs] = (a_x + b_x) * 0.5;
      edge_midpoints[edge * num_dims + y_ofs] = (a_y + b_y) * 0.5;

      /* vector from first to last point */
      v_x = b_x - a_x;
      v_y = b_y - a_y;

      /* two-dimensional (unary) cross product analog that makes the
       * "triple" (dot-cross) product zero, i.e. it's a normal; the
       * direction of this vector is such that it will be pointing
       * inwards when enumerating nodes clock-wise */
      edge_normals[edge * num_dims + x_ofs] = +v_y;
      edge_normals[edge * num_dims + y_ofs] = -v_x;

      /* Euclidian norm in two dimensions is magnitude of edge */
      edge_lengths[edge] = sqrt(v_x*v_x + v_y*v_y);
   }
}

/* ------------------------------------------------------------------ */
void
compute_face_geometry(int ndims, double *coords, int nfaces,
                      int *nodepos, int *facenodes, double *fnormals,
                      double *fcentroids, double *fareas)
/* ------------------------------------------------------------------ */
{
   if (ndims == 3)
   {
      compute_face_geometry_3d(coords, nfaces, nodepos, facenodes,
                               fnormals, fcentroids, fareas);
   }
   else if (ndims == 2)
   {
      /* two-dimensional interfaces are called 'edges' */
      compute_edge_geometry_2d(coords, nfaces, nodepos, facenodes,
                               fnormals, fcentroids, fareas);
   }
   else
   {
      assert(0);
   }
}


/* ------------------------------------------------------------------ */
static void
compute_cell_geometry_3d(double *coords,
                         int *nodepos, int *facenodes, int *neighbors,
                         double *fnormals,
                         double *fcentroids,
                         int ncells, int *facepos, int *cellfaces,
                         double *ccentroids, double *cvolumes)
/* ------------------------------------------------------------------ */
{
   const int ndims = 3;
   int i,k, f,c;
   int face,node;
   double x[3];
   double u[3];
   double v[3];
   double w[3];
   double xcell[3];
   double ccell[3];
   double cface[3]  = {0};
   const double twothirds = 0.666666666666666666666666666667;

   int ndigits;

   ndigits = ((int) (log(ncells) / log(10.0))) + 1;


   for (c=0; c<ncells; ++c)
   {
      int num_faces;
      double volume = 0.0;

      for(i=0; i<ndims; ++i) xcell[i] = 0.0;
      for(i=0; i<ndims; ++i) ccell[i] = 0.0;


      /*
       * Approximate cell center as average of face centroids
       */
      for(f=facepos[c]; f<facepos[c+1]; ++f)
      {
         face = cellfaces[f];
         for (i=0; i<ndims; ++i) xcell[i] += fcentroids[3*face+i];
      }
      num_faces = facepos[c+1] - facepos[c];

      for(i=0; i<ndims; ++i) xcell[i] /= num_faces;



      /*
       * For all faces, add tetrahedron's volume and centroid to
       * 'cvolume' and 'ccentroid'.
       */
      for(f=facepos[c]; f<facepos[c+1]; ++f)
      {
         int num_face_nodes;

         for(i=0; i<ndims; ++i) x[i] = 0.0;
         for(i=0; i<ndims; ++i) cface[i] = 0.0;

         face = cellfaces[f];

         /* average face node x */
         for(k=nodepos[face]; k<nodepos[face+1]; ++k)
         {
            node = facenodes[k];
            for (i=0; i<ndims; ++i) x[i] += coords[3*node+i];
         }
         num_face_nodes = nodepos[face+1] - nodepos[face];
         for(i=0; i<ndims; ++i) x[i] /= num_face_nodes;



         /* compute first vector u (to the last node in the face) */
         node = facenodes[nodepos[face+1]-1];
         for(i=0; i<ndims; ++i) u[i] = coords[3*node+i] - x[i];


         /* Compute triangular contributions to face normal and face centroid */
         for(k=nodepos[face]; k<nodepos[face+1]; ++k)
         {
            double tet_volume, subnormal_sign;
            node = facenodes[k];
            for (i=0; i<ndims; ++i) v[i] = coords[3*node+i] - x[i];

            cross(u,v,w);

            tet_volume = 0;
            for(i=0; i<ndims; ++i){
                tet_volume += w[i] * (x[i] - xcell[i]);
            }
            tet_volume *= 0.5 / 3;

            subnormal_sign=0.0;
            for(i=0; i<ndims; ++i){
                subnormal_sign += w[i]*fnormals[3*face+i];
            }

            if (subnormal_sign < 0.0) {
                tet_volume = - tet_volume;
            }
            if (neighbors[2*face + 0] != c) {
                tet_volume = - tet_volume;
            }

            volume += tet_volume;

            /* face centroid of triangle  */
            for (i=0; i<ndims; ++i) cface[i] = (x[i]+twothirds*0.5*(u[i]+v[i]));

            /* Cell centroid */
            for (i=0; i<ndims; ++i) ccell[i] += tet_volume * 3/4.0*(cface[i] - xcell[i]);


            /* Store v in u for next iteration */
            for (i=0; i<ndims; ++i) u[i] = v[i];
         }
      }

      if (! (volume > 0.0)) {
          fprintf(stderr,
                  "Internal error in mex_compute_geometry(%*d): "
                  "negative volume\n", ndigits, c);
      }

      for (i=0; i<ndims; ++i) ccentroids[3*c+i] = xcell[i] + ccell[i]/volume;
      cvolumes[c] = volume;
   }
}

/* ------------------------------------------------------------------ */
void
compute_cell_geometry(int ndims, double *coords,
                      int *nodepos, int *facenodes, int *neighbors,
                      double *fnormals,
                      double *fcentroids,
                      int ncells, int *facepos, int *cellfaces,
                      double *ccentroids, double *cvolumes)
/* ------------------------------------------------------------------ */
{
   if (ndims == 3)
   {
      compute_cell_geometry_3d(coords, nodepos, facenodes,
                               neighbors, fnormals, fcentroids, ncells,
                               facepos, cellfaces, ccentroids, cvolumes);
   }
   else
   {
      assert(0);
   }
}
