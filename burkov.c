#include "burkov.h"

void (*dicho_tree) (node_t*, const int, item_t*) = dicho_tree_notrecursive;
node_t* (*burkovtree)(const task_t*) = optimal_dichotomic_tree;
void (*treesolver) (node_t*, knint) = notrecursive_treesolver;


/*
*  building optimum dichotomy tree on Burkov,Burkova's works
*/
node_t* optimal_dichotomic_tree ( const task_t *task){

  //{ finding q <= countItems - number of elements on which the maximal symmetric hierarchy must be created
    int q = find_q (task->b);
    q = (q > task->length) ? task->length : q;
  //}

  // maximum symmetric hierarchy must be created from top to down!
  // optimally sorting items
  item_t *diitems, // items for dichotomic part
         *dpitems; // items for dynamic programming branch
  prep_items(task->length, task->items, q, &diitems, &dpitems);

  // head of optimal dichotomic tree
  node_t* head = createnodes (2*task->length-1); // number of all nodes of any tree is doubled number of it's leafs minus one.
  head->hnode = NULL; // parentof the tree's head

  // DP branch
  node_t *p = head;
  item_t *pl = dpitems;//, *tmp;
  int i;
  for ( i = 0 ; i < task->length-q ; i++ ) { // in fact p move as p = p + 2
    p->lnode = (p+1);
    //paired with hash p->lnode->items = NULL;
    //tmp = copyitem(pl);
    //HASH_ADD_KEYPTR ( hh, p->lnode->items, tmp->w, KNINT_SIZE, tmp);
    p->lnode->items = copyitem(pl);
    pl++;
    p->lnode->length = 1;
    p->lnode->hnode = p;

    p->rnode = (p+2);
    p->rnode->hnode = p;

    p = p->rnode;
  }

  // Dichotomic branch
  dicho_tree(p, q, diitems);

  return head;
}

// must be: q <= size
// diitems - items for dichotomic part of tree
// dpitems - items for dynamic programming part of tree
void prep_items ( const int size, item_t *list, const int q, item_t **diitems, item_t **dpitems ){
  // it's just a plug
  (*dpitems) = list;
  (*diitems) = list + (size-q); // dangerous:
}

// b(q)
knint bq[] = {0, 0, 4, 6, 12, 20, 38, 70, 140, 268, 532, 1044, 2086, 4134, 8262, 16454, 32908, 65676, 131340, 262412, 524820, 1049108, 2098196, 4195348, 8390694,
   16779302, 33558566, 67112998, 134225990, 268443718, 536887366, 1073758278, 2147516556, 4295000204, 8590000268, 17179934860, 34359869708, 68719608076, 137439215884, 274878169356};
const short bq_size = sizeof(bq) / sizeof(bq[0]);
int find_q (knint b) {
  knint *i = bq+2;
  for ( ; (i<bq+bq_size && (*i) < b ) ; i++ ) {}
  return (i-bq);
}

void dicho_tree_notrecursive(node_t *head, const int size, item_t *items){
  int p_size = 1, pnext_size = 2;
  node_t *p = head, *pnext, *t1, *t2;
  int *sizes = (int*)malloc(2*sizeof(int)), // size of accordingly subtree items
      *indexes = sizes + 1, // start index of items that belongs to accordingly subtree
      *tmpsizes, *tmpindexes, *ss, *ii, *ss2, *ii2;
    *sizes = size; *indexes = 0;

  // creating a maximum symmetric tree
  int dp = (int)log2f ((float)size); // tree's depth
  int i;
  for ( i = 0 ; i < dp ; i++ ){
    //1: pnext = (node_t*)calloc (pnext_size, NODE_SIZE); // the next level of tree
    pnext = p + p_size; // the next level of tree
    ss = (int*)malloc (2*pnext_size*sizeof(int));
    ii = ss + pnext_size;

    t2 = pnext;
    ss2 = ss;
    ii2 = ii;
    tmpsizes = sizes; // for free()
    //tmpindexes = indexes; // for free()
    int j = 0, d;
    for( t1 = p ; t1 < p + p_size ; t1++ ){
      d = (*sizes) / 2;
        *ss2 = d;
        *(ss2+1) = (*sizes) - d;
        ss2 = ss2 + 2;
        *ii2 = (*indexes);
        *(ii2+1) = (*indexes) + d;
        ii2 = ii2 + 2;
        sizes++;
        indexes++;

      t1->lnode = t2;
      t2->hnode = t1; t2++;
      t1->rnode = t2;
      t2->hnode = t1; t2++;
      j++;
    }
    sizes = ss;
    indexes = ii;
    free (tmpsizes);
    //free(tmpindexes); commented because ss = malloc; ii = ss + ...

    p = pnext;
    p_size = pnext_size;
    pnext_size <<= 1;
  } // for i

  // hang up all remaining items (where sizes[i]==2)
  //t2 = p;
  item_t *tmp;
  pnext = p + p_size;
  for( i = 0 ; i < p_size ; i++ ){
    if( sizes[i] > 2 ) { printf("size of leaf more than 2\n"); fflush(stdout); }
    if( sizes[i] == 2 ){
      //1: pnext = (node_t*)calloc (2,NODE_SIZE);
      //pnext->items = NULL;
      //tmp = copyitem (&items[indexes[i]]);
      //HASH_ADD_KEYPTR (hh, pnext->items, tmp->w, KNINT_SIZE, tmp);
      pnext->items = copyitem (&items[indexes[i]]);
      pnext->length = 1;
      p->lnode = pnext;
      pnext->hnode = p; pnext++;

      //pnext->items = NULL;
      //tmp = copyitem (&items[indexes[i]+1]);
      //HASH_ADD_KEYPTR (hh, pnext->items, tmp->w, KNINT_SIZE, tmp);
      pnext->items = copyitem (&items[indexes[i]+1]);
      pnext->length = 1;
      p->rnode = pnext;
      pnext->hnode = p; pnext++;
    } else if( sizes[i] == 1 ){
      //p->items = NULL;
      //tmp = copyitem (&items[indexes[i]]);
      //HASH_ADD_KEYPTR (hh, p->items, tmp->w, KNINT_SIZE, tmp);
      p->items = copyitem (&items[indexes[i]]);
      p->length = 1;
    } else {
      // error
      printf("size of leaf is non-positive?\n");
      fflush(stdout);
    }
    p++;
  }

}// dicho_tree_notrecursive()

/*  funcs for solving */

void notrecursive_treesolver ( node_t* root, knint cons ){
  node_t *runner = root, *smallnode, *bignode;
  if ( root->rnode == NULL || root->lnode == NULL ) return;
  int depth = 0;
  while ( runner != NULL ) {
  	while ( (runner->rnode->length == 0) || (runner->lnode->length == 0) ) {
  		while ( runner->rnode->length == 0 ) { runner = runner->rnode; depth++; }
  		while ( runner->lnode->length == 0 ) { runner = runner->lnode; depth++; }
  	}

	if ( runner->lnode->length > runner->rnode->length ) {
		bignode = runner->lnode;
		smallnode = runner->rnode;
	} else {
		bignode = runner->rnode;
		smallnode = runner->lnode;
	}

	dichosolve (runner, bignode, smallnode, cons );

	//clear_node (runner->lnode);
        //clear_node (runner->rnode);

  	printf ( "depth = %d. right length = %d, left length = %d\n", depth, runner->rnode->length, runner->lnode->length ); 
  	//print_hash (runner->rnode->items);
  	//print_hash (runner->lnode->items);
  	fflush (stdout);

  	runner = runner->hnode;
  	depth--;
  } // while
}

void recursive_treesolver(node_t* root, knint cons){
  if( root->length != 0 ) return;

  treesolver (root->lnode,cons);
  treesolver (root->rnode,cons);

  if ( root->lnode->length > root->rnode->length )
  	dichosolve(root, root->lnode, root->rnode, cons );
  else
  	dichosolve(root, root->rnode, root->lnode, cons );
//  print_tree(root);
}


void dichosolve ( node_t* to, node_t* big, node_t* small, knint cons ) {

  to->items = big->items;
  to->length = big->length;
  
  // big->items = NULL;

  if ( small->length < 1 ) { return; }

  //item_t *its = createitems0 (cons), *fp, *sp;
  item_t *fp, *sp, *tmp;
  knint w, p;
  
  puts ("dichosolve. pairwise addition"); fflush(stdout);
  // pairwise addition
  item_t *lastelem, *preelem = NULL;
  for( fp = to->items ; fp != NULL ; preelem = fp, fp = fp->next ) {
    if ( fp->flag == NEW_ELEM ) { fp->flag = OLD_ELEM; continue; }
    lastelem = fp;
    for( sp = small->items ; sp != NULL && (p = *(fp->p) + *(sp->p), w = *(fp->w) + *(sp->w), w<=cons) ; sp = sp->next ) {
    	lastelem = find_preplace_badcutter (lastelem,&w, &(to->length));
    	tmp = copyitem (lastelem);
    	*(tmp->p) = p;
    	*(tmp->w) = w;
    	put_item (lastelem, &tmp, &(to->length));
    } // for sp
    if ( fp->flag == ONESHOT_ELEM ) {
    	preelem->next = fp->next; // preelem isn't NULL cause ONESHOT_ELEM cann't be a first, see put_item().
    	free_items (&fp);
    	fp = preelem;
    	to->length--;
    }
  } // for fp

  puts("put new elements of second table or replace elements having less value");fflush(stdout);
// after this error fireup
  item_t *desert = createitems0(1);
  lastelem = to->items;
  fp = small->items;
  small->items = small->items->next;
  if ( (tmp = find_preplace_badcutter(lastelem, small->items->w, &(to->length))) == NULL ) {
	fp->flag = OLD_ELEM;
	to->items = fp;
	to->items->next = lastelem;
	lastelem = to->items;
	to->length++;
  } else {
	if ( safe_put_item (tmp, &fp, &(to->length)) == 0 ) {
		fp->flag = OLD_ELEM;
		lastelem = fp;
	} else { // put_item drops fp
		lastelem = tmp;
		fp->next = desert->next;
		desert->next = fp;
	}
  }

  puts ("cycle"); fflush(stdout);
  for( fp = small->items ; fp != NULL /*&& *(fp->w) <= cons*/ ; ) {
    lastelem = find_preplace_badcutter (lastelem, fp->w, &(to->length));
    tmp = fp->next;
    if ( safe_put_item (lastelem, &fp, &(to->length)) == 0 ) {
    	fp->flag = OLD_ELEM;
    } else {
    	fp->next = desert->next;
	desert->next = fp;
    }
    fp = tmp;
        
  }
  small->items = desert->next; // hold bad items (unsorted?)
  free_items (&desert);

  puts("// delete inefficient elems in tail");fflush(stdout);
	knint edge;
	do {
		edge = *(lastelem->p);
		while ( lastelem->next != NULL && edge >= *(lastelem->next->p) ) {
			tmp = lastelem->next;
			lastelem->next = lastelem->next->next;
			free_items (&tmp);
			to->length--;
		}
		
		lastelem = lastelem->next;
	} while ( lastelem );

} // dichosolve()