#include "mpi.h"
#include "mpix.h"

/*
  Source is at middle of partition.
  Using the abcde coordinate of this node, we iterate through the neighbor
  nodes that are 1 hop away in + and - direction of each dimension, and assign
  those as neighbors.
*/    

int getranks(int N, int *source, int **targets, int * ppn)
{
  int i, j, c = 0;
  MPIX_Hardware_t hw;
  MPIX_Hardware(&hw);
  *ppn = hw.ppn;
  
  int * coords = (int*) malloc(sizeof(int) * (hw.torus_dimension+1));
  int * tmp = (int*) malloc(sizeof(int) * (hw.torus_dimension+1));
  
  // assign source node, which is at center of partition
  for (i = 0; i < hw.torus_dimension; i++) coords[i] = hw.Size[i] / 2;
  coords[i] = hw.coreID;
  MPIX_Torus2rank(coords, source);

  *targets = (int*) malloc(sizeof(int) * N);
  int *ptr = *targets;

  while (c < N)
  {
      for (j = 0; j < hw.torus_dimension + 1; j++) tmp[j] = coords[j];
      if (c < 5)
      {
        tmp[c] = (coords[c] + 1) % hw.Size[c];
        MPIX_Torus2rank(tmp, &ptr[c++]);
      }
      else
      {
        tmp[c-5] = (coords[c-5] - 1 + hw.Size[c-5]) % hw.Size[c-5];
        MPIX_Torus2rank(tmp, &ptr[c++]);
      }
  }

  free(tmp);
  free(coords);
  return 0;
}
