#include "mpi.h"
#include "mpix.h"

/*
  Returns the source task and the list of the nearest target tasks (1 hop
  distance) For partition (X,Y,Z), the source is the task, placed to
  (x/2,y/2,z/2,t) to guarantee the maximal connectivity t is the smallest
  available.
  
  The target[N] list is x+-1,y,z,t or x,y+-1,z,t or x,y,z+-1,t
    
   Returns:
   0, if source and target are assigned
   1, if source node does not contain tasks
   2, if one of the target nodes does not contain tasks
   3, if N != 9, BG/Q 3D mesh/torus
*/

MPIX_Hardware_t hw;
int *coords, *tmp;
int rank;
int _src;

void freeMem()
{
  free(coords);
  free(tmp);
}
void init(int *ppn, int * source)
{
  int i;
  MPIX_Hardware(&hw);
  coords = (int*) malloc(sizeof(int) * (hw.torus_dimension+1));
  tmp = (int*) malloc(sizeof(int) * (hw.torus_dimension+1));

  *ppn = hw.ppn;
  
  // assign source node, which is at center of partition
  for (i = 0; i < hw.torus_dimension; i++) coords[i] = hw.Size[i] / 2;

  coords[i] = 0;

  MPIX_Torus2rank(coords, source);
  * source = _src = 0;
  MPIX_Rank2torus(*source, &coords[0]);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
}

void getranks(int XNbors, int *targets)
{
  int i, j, k = 0;
  int rank;
  /*
  i = 0; j = 4;
  memcpy(tmp, coords, sizeof(int) * (hw.torus_dimension+1));
  tmp[i] = (coords[i] + 1) % hw.Size[i];
  tmp[5]=0;
      MPIX_Torus2rank(tmp, &targets[k++]);
  memcpy(tmp, coords, sizeof(int) * (hw.torus_dimension+1));
  tmp[j] = (coords[j] + 1) % hw.Size[j];
  tmp[5]=0;
      MPIX_Torus2rank(tmp, &targets[k++]);
  
  return;
  */
  for (i = 0; i < XNbors * hw.ppn; i++)
    targets[i] = _src + hw.ppn + i;
  return;
  for (i = 0; i < XNbors; i++)
  {
    memcpy(tmp, coords, sizeof(int) * (hw.torus_dimension+1));
    
    tmp[3] = (coords[3] + 1) % hw.Size[3];
#if 1
    if (i < 5) // A, B, C, D, E: + direction nbors
      tmp[i] = (coords[i] + 1) % hw.Size[i];
    else // A, B, C, D: - direction nbors
      tmp[i%hw.torus_dimension] = (coords[i%hw.torus_dimension] - 1 +
                                   hw.Size[i%hw.torus_dimension]) %
                                  hw.Size[i%hw.torus_dimension];
#endif
    // iterate thru the ppn and create a target
    for (j = 0; j < hw.ppn; j++)
    {
      tmp[hw.torus_dimension] = j;
      MPIX_Torus2rank(tmp, &targets[k++]);
    }     
  }

}
