#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "reversi.h"
#include "book.h"

tree *root,*nfree,*fnode,*blackroot,*whiteroot;
long aalloc,maxalloc,nodes;
short int playnm,treeon;
gamerec game;
/************************************************/
/* readlib konverterer biblioteket til treaform */
/************************************************/

void savelib(tree * root,FILE * f)
{
  short int children = 0;
	short int value = 0, flag = NOT_CALCULATED;
  tree *temp;

  if (!root) {
    fwrite (&children, sizeof (children), 1, f);
    return;
  }
	temp = root;
	while (temp->sosk != NULL) {
		temp = temp->sosk;
		children ++;
	}
	children ++;
	nodes += children;
	fwrite (&children, sizeof (children), 1, f);

	fwrite (&(root->move), sizeof (root->move), 1, f);
	fwrite (&(value), sizeof (value), 1, f);
	fwrite (&(flag), sizeof (flag), 1, f);

  savelib(root->barn,f);

  temp = root;
  while (--children > 0) {
    temp = temp->sosk;
	  fwrite (&(temp->move), sizeof (temp->move), 1, f);
	  fwrite (&(value), sizeof (value), 1, f);
	  fwrite (&(flag), sizeof (flag), 1, f);
    savelib(temp->barn,f);
  }
}


/*******************************************************************************/
/* Get_lib laeser biblioteket ind fra disken, og konverterer det saadan at de  */
/* spejlede versioner bliver dannet                                            */
/*******************************************************************************/

int Put_lib(void)
{
  FILE * f;

  if ((f = fopen("oldopening","w")) != NULL) {
			fwrite (&nodes, sizeof (nodes), 1, f);
      savelib(blackroot,f);
			fclose (f);
  }
  else
    return FALSE;

  return TRUE;
}

/************************************************/
/* readlib konverterer biblioteket til treaform */
/************************************************/

void mlib(tree ** root,char* *src)
{
  short int children;
	char mov;
  tree *temp;

  if ((children = *(*src)++) == 0) 
    return;
	
  if (*root == NULL) {
		make_node(root);
		(*root)->move = *(*src)++;
		mlib(&(*root)->barn,src);
	
		temp = *root;
		while (--children > 0) {
			make_node(&temp->sosk);
			temp = temp->sosk;
			temp->move = *(*src)++;
			mlib(&temp->barn,src);
		}
	}
	else {
		while (--children > 0) {
			mov = *(*src)++;
			temp = *root;
			while (temp->sosk != NULL && temp->move != mov)
				temp = temp->sosk;
			if (temp->move != mov) {
				make_node(&temp->sosk);
				temp = temp->sosk;
				temp->move = mov;
			}
			mlib(&temp->barn,src);
		}
	}
}



void readlib(tree ** root,char* *src);
/*******************************************************************************/
/* Get_lib laeser biblioteket ind fra disken, og konverterer det saadan at de  */
/* spejlede versioner bliver dannet                                            */
/*******************************************************************************/

int Merge_lib(char * whitelib, char * blacklib)
{
  char buffer[6000],*src;
  FILE * f;

  if ((f = fopen(blacklib,"r")) != NULL) {
    if (fread(&buffer,1,sizeof(buffer), f) > 0) {
      src = buffer;
      readlib(&blackroot,&src);
    }
    else
      return FALSE;
  }
  else
    return FALSE;

  if ((f = fopen(whitelib,"r")) != NULL) {
    if (fread(&buffer,1,sizeof(buffer), f) > 0) {
      src = buffer;
      mlib(&blackroot,&src);
    }
    else
      return FALSE;
  }
  else
    return FALSE;
		
  maxalloc -= aalloc;
  aalloc = 0;
  return TRUE;
}

main ()
{
    init_nodes(100000);
		Merge_lib("white.lib", "black.lib");
		nodes = 0;
		Put_lib();
		printf("Nodes = %d\n",nodes);
}
