#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

#define NO_DEBUG 0
#define LOW_DEBUG 1
#define MID_DEBUG 2
#define HI_DEBUG 3

#define DBGLVL HI_DEBUG

#define SIZE_MSG 100
#define P_MSG 101
#define W_MSG 102
#define B_MSG 103

#if defined(KNINT_INT)
#define MPI_KNINT MPI_INT
#elif defined(KNINT_LONG)
#define MPI_KNINT MPI_LONG
#endif

int main(int argc, char** argv){
  // mpi headers
  MPI_Init (&argc, &argv);
  int groupsize, myrank;
  MPI_Comm_size (MPI_COMM_WORLD, &groupsize);
  MPI_Comm_rank (MPI_COMM_WORLD, &myrank);

  if ( argc < 2 ){
    MPI_Finalize ();
    printf("Task #%d say: not enough arguments: filename needed\n",myrank);
    exit(1);
  }

  task_t *mytask;

  MPI_Request *msgreq = (MPI_Request*)malloc(4*sizeof(MPI_Request)), *reqsz = msgreq, *reqp = msgreq+1, *reqw = msgreq+2, *reqb = msgreq+3;
  MPI_Status *msgstat = (MPI_Status*)malloc(4*sizeof(MPI_Status)), *statsz = msgstat, *statp = msgstat+1, *statw = msgstat+2, *statb = msgstat+3;

  node_t *root = NULL;

/* get task code for process zero */
if (myrank == 0)
{
  mytask = readnspread_task_parallel (argv[1], &groupsize);
}

/* get task code for process one */
else {
    // get b
    knint b;
    MPI_Bcast (&b,1,MPI_KNINT,0,MPI_COMM_WORLD);

    // get size of elements
    int size;
    MPI_Recv (&size,1,MPI_INT,0,SIZE_MSG,MPI_COMM_WORLD, statsz);
    if( size < 1 ){
      free (msgreq);
      free (msgstat);
      MPI_Finalize ();
      return 0;
    }

    // get values and weights of elements
    mytask = createtask (size,b);
    MPI_Irecv (mytask->items->p,size,MPI_KNINT,0,P_MSG,MPI_COMM_WORLD, reqp);
    MPI_Irecv (mytask->items->w,size,MPI_KNINT,0,W_MSG,MPI_COMM_WORLD, reqw);
    MPI_Waitall (2,reqp,statp);

    #if DBGLVL >= LOW_DEBUG
    printf("%d readed. b=%ld, size=%d.\n",myrank,mytask->b,mytask->length); fflush(stdout);
    #endif

    #if DBGLVL >= LOW_DEBUG
    puts("build tree..."); fflush(stdout);
    #endif
    //{ solve mytask
    if( (root = burkovtree ( mytask )) == 0 )
    { puts("Can't build optdichotree"); fflush(stdout); }

    #if DBGLVL >= HI_DEBUG
    print_tree (root); fflush(stdout);
    #endif

    #if DBGLVL >= LOW_DEBUG
    puts("solve local task..."); fflush(stdout);
    #endif
    treesolver (root,mytask->b);

    #if DBGLVL >= LOW_DEBUG
      printf("%d: local task solved. Solving parallel...\n",myrank); fflush(stdout);
    #endif
    //}

    #if DBGLVL >= HI_DEBUG
      print_tree(root); fflush(stdout);
    #endif

}// else

  // get elements(solutions) from other processes and solve again with it
  int frontier = groupsize, group;
  int cnt = 0;

  while ( frontier > 1 && myrank < frontier ){
    group = frontier;
    frontier = group / 2;

    if( myrank < frontier ){
      root = receive_brother (myrank+frontier, root, &cnt, statsz);
      if( (myrank + 1 == frontier) && (group % 2 == 1) ){
        root = receive_brother (group-1, root, &cnt, statsz);
      }

      #if DBGLVL >= HI_DEBUG
        if ( myrank == 0 ) { 
        	puts("-"); 
        	if ( root->rnode != NULL && root->lnode != NULL && root->rnode->length > 0 && root->lnode->length > 0 ) {
        		printf ("root length = %d, received length = %d\n", root->rnode->length, root->lnode->length);
        	}
        	fflush(stdout);
        }
      #endif
      // solve them all
      #if DBGLVL >= MID_DEBUG
        printf("process %d are solving received tree..\n", myrank); fflush(stdout);
      #endif
      treesolver (root, mytask->b);
      #if DBGLVL >= MID_DEBUG
        printf("process %d solved received tree.\n", myrank); fflush(stdout);
      #endif
    }
  }

  // if process zero is ended previous code then complete solution is obtained, print it
  if ( myrank == 0 ) {
    if ( root->length == -1 ) { puts("length == -1"); fflush(stdout); }
    else {
      if ( root->items == NULL ) puts ("Wwarning! There's no items!");
      if ( root->items->p == NULL ) puts("Wwarning! There's no solutions!");
      // sorting
      #if DBGLVL >= MID_DEBUG
        puts("sorting..."); fflush(stdout);
      #endif
      HASH_SORT ( root->items, value_sort );
      // print it
      printf ( "knapsack: (p=%ld w=%ld)\n", *(root->items->p), *(root->items->w) ); //fflush(stdout);
    }

  }
  // other processes must send their solutions to appropiate process
  else {
    int to = myrank - frontier;
    if( (myrank+1 == group) && (group % 2 == 1) ) to = frontier - 1;
    // the number of solution elements
    MPI_Isend(&(root->length), 1, MPI_INT, to, SIZE_MSG, MPI_COMM_WORLD, reqsz);
    if( root->length > 0 ){
      knint *p = (knint*)malloc(root->length*KNINT_SIZE), *w = (knint*)malloc(root->length*KNINT_SIZE), *pp, *pw;
      item_t *fp;
      for ( fp = root->items, pp = p, pw = w ; fp != NULL ; fp = fp->hh.next, pp++, pw++ ){
        *pp = *(fp->p);
        *pw = *(fp->w);
      }
      // send p and w
      MPI_Isend( p, root->length, MPI_KNINT, to, P_MSG, MPI_COMM_WORLD, reqp);
      //printf("%d send: ",myrank); print_items(root->length, root->items);
      MPI_Isend( w, root->length, MPI_KNINT, to, W_MSG, MPI_COMM_WORLD, reqw);
      MPI_Waitall(2,reqp,statp);
      free(p); free(w);
    }
    MPI_Wait(reqsz,statsz);

  } // else of if myrank == 0

  #if DBGLVL >= LOW_DEBUG
  printf ("%d: finalizing...\n",myrank); fflush (stdout);
  #endif

  #if DBGLVL >= LOW_DEBUG
  puts ("free tree"); fflush(stdout);
  #endif
  node_t *t;
  for( ; cnt > 0 ; cnt-- ){
    t = root;
    root = root->lnode;
    free_node (t);
  }
  free (root);//free_tree (root);

  #if DBGLVL >= LOW_DEBUG
    puts ("free some var"); fflush(stdout);
  #endif
  free_task(&mytask);
  free(msgreq);
  free(msgstat);

  #if DBGLVL >= LOW_DEBUG
  	printf("%d: ok\n", myrank); fflush (stdout);
  #endif
  // mpi tails
  MPI_Finalize ();
  return 0;
} // main()

node_t* receive_brother(int from, node_t *root, int *cnt, MPI_Status *stat){
  node_t *head = (root == NULL) ? createnodes(1) : createnodes(2), *thead = (root==NULL)?(head):(head+1);

  MPI_Recv(&(thead->length), 1, MPI_INT, from, SIZE_MSG, MPI_COMM_WORLD, stat);

  if ( thead->length > 0 ) {
    knint *p = (knint*)malloc(thead->length*KNINT_SIZE), *w = (knint*)malloc(thead->length*KNINT_SIZE), *pp, *ww;
    MPI_Recv(p, thead->length, MPI_KNINT, from, P_MSG, MPI_COMM_WORLD, stat);
    MPI_Recv(w, thead->length, MPI_KNINT, from, W_MSG, MPI_COMM_WORLD, stat);
    thead->items = NULL;
    item_t *it = createitems(1);
    for ( pp = p, ww = w ; pp < p + thead->length ; pp++, ww++ ) {
      *(it->p) = *pp;
      *(it->w) = *ww;
      HASH_ADD_KEYPTR (hh, thead->items, it->w, KNINT_SIZE, it);
      it = copyitem (it);
    }
    thead->source = from;
    free(p); free(w);
  } else {
    thead->source = -1;
  }

  (*cnt)++;

  if ( root != NULL ) {
      head->lnode = root;
      head->rnode = thead;
      root->hnode = head;
      thead->hnode = head;
  }

  return head;
}

/*
    read the task from "filename" file and parallel divide it within group of "groupsize" size
*/
task_t* readnspread_task_parallel(char* filename, int *groupsize){
    task_t *mytask;
    MPI_Request *reqs = (MPI_Request*)malloc((*groupsize)*sizeof(MPI_Request));
    MPI_Status *stats = (MPI_Status*)malloc((*groupsize)*sizeof(MPI_Status));

    FILE *file;
    if( (file = fopen(filename,"r")) == 0 ) return 0;

    int i;
    knint b;
  // puts("send b"); fflush(stdout);
    if( fscanf(file,"%ld",&b) != 1 ) return 0;
    MPI_Bcast (&b, 1, MPI_KNINT, 0, MPI_COMM_WORLD);
    //MPI_Waitall ((*groupsize)-1,reqs+1,stats+1);

  // puts("send size"); fflush(stdout);
    int size;
    if( fscanf(file,"%d",&size) != 1 ) return 0;
    // printf("Size=%d\n",size); fflush(stdout);
      // if groupsize > size then to half groupsize
      int oldgroup = *groupsize;
      if( size < *groupsize ) {  *groupsize = size / 2;  }
    int onesize = size / (*groupsize-1), rest,
        *sizes = (int*)malloc((*groupsize)*sizeof(int)), *psz;

    rest = (size % (*groupsize-1)); // residue elements
    // i(rank 0) am not in the group of solving base tasks
    for( i = 1, psz = sizes+1 ; i < *groupsize ; i++, psz++ ){
      (*psz) = onesize + (rest>0?1:0);
      MPI_Send (psz, 1, MPI_INT, i, SIZE_MSG, MPI_COMM_WORLD);
      rest--;
    }

  // puts("read p"); fflush(stdout);
    knint **ps = (knint**) malloc ((*groupsize)*sizeof(knint*)),
          **ws = (knint**) malloc ((*groupsize)*sizeof(knint*)), **tmps, **tmws;
    for ( i = 1,tmps=ps+1,tmws=ws+1 ; i < *groupsize ; i++,tmps++,tmws++ ) {
      *tmps = (knint*)malloc(sizes[i]*KNINT_SIZE);
      *tmws = (knint*)malloc(sizes[i]*KNINT_SIZE);
    }

      int proc = 1, num = 0;
      for( i = 0 ; i < size ; i++ ) {
        if( fscanf (file,"%ld", ps[proc]+num) != 1 ) return 0;
        proc++;
        if( proc == *groupsize ){
          proc = 1;
          num++;
        }
      }
  // puts("sending p"); fflush(stdout);
    knint *p;
      for( i = 1 ; i < *groupsize ; i++ ) {
        MPI_Isend (ps[i], sizes[i], MPI_KNINT, i, P_MSG, MPI_COMM_WORLD, reqs+i);
      }

  // puts("mytask"); fflush(stdout);
      mytask = createtask(0,b);

  // puts("read w"); fflush(stdout);
      proc = 1;
      num = 0;
      for( i = 0 ; i < size ; i++ ) {
        if( fscanf (file,"%ld", ws[proc]+num) != 1 ) return 0;
        proc++;
        if( proc == *groupsize ){
          proc = 1;
          num++;
        }
      }
  // sending w
      MPI_Waitall ((*groupsize)-1,reqs+1,stats+1);
      for( i = 1 ; i < *groupsize ; i++ ) {
        MPI_Isend (ws[i], sizes[i], MPI_KNINT, i, W_MSG, MPI_COMM_WORLD, reqs+i);
      }

  // puts("close unnecessary workers"); fflush(stdout);
    //MPI_Waitall ((*groupsize)-1,reqs+1,stats+1);
    rest = -1;
    for( i = *groupsize ; i < oldgroup ; i++ ){
      MPI_Isend (&rest,1,MPI_INT, i, SIZE_MSG, MPI_COMM_WORLD, reqs+i);
    }

    for( i = 1 ; i < *groupsize ; i++ ) { free(ps[i]); free(ws[i]); }
    free (ps); free (ws);

    fclose(file);
    free (reqs);
    free (stats);
    free (sizes);
    return mytask;
} // readnspread_task_parallel()
