#include "stdafx.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <time.h>
#include "reversi.h"
#include "book.h"
#include "..\stdafx.h"
#include "..\Stello.h"
#include "..\StelloDoc.h"
#include "..\StelloView.h"

extern CStelloView * m_pStelloView;

#define LIBLEVEL 11

//time_t time(time_t *tloc);

booktree *book_root,*nbfree,*fbnode;
long abook,maxbook,libmoves;
extern short int calclib, tryagain;
short int libok,lowlook; 
jmp_buf libenv;

char f_legalmove(short int pl, short int trek, board *bd);

/*******************************************************************************/
/* make_node laver en dummy_knude, bruges naar en spiller ikke har noget treak */
/*******************************************************************************/
void make_book_node(booktree* *spiltree)
{
  booktree *temp;

  temp = nbfree; 
  (*spiltree) = temp;
  
  nbfree = temp->barn;
  /* nfree peger nu paa naeste frie knude */
  
  if (++abook > maxbook)
    /* hvis der nu ikke er plads til flere knuder saettes treeon til falsk */
    treeon = FALSE;
  temp->move = 0; 
	temp->value = 0;
	temp->flag = 0;
  temp->barn = temp->sosk = NULL;
} 



/************************************************/
/* readlib konverterer biblioteket til treaform */
/************************************************/

void readbook(booktree ** root,FILE * f)
{
  short int children = 0;
  booktree *temp;
	int readbyt;
	
	readbyt = fread (&children, sizeof (children), 1, f);
	// children = ntohs (children);

  if (children == 0) {
		*root = NULL;
    return;
	}

  make_book_node(root);
  fread (&((*root)->move ), sizeof ((*root)->move ), 1, f);
//	(*root)->move = ntohs ((*root)->move);
  fread (&((*root)->value), sizeof ((*root)->value), 1, f);
//	(*root)->value = ntohs ((*root)->value);
  fread (&((*root)->flag), sizeof ((*root)->flag), 1, f);
//	(*root)->flag = ntohs ((*root)->flag);

  readbook(&(*root)->barn,f);

  temp = *root;
  while (--children > 0) {
    make_book_node(&temp->sosk);
    temp = temp->sosk;
    fread (&(temp->move), sizeof (temp->move), 1, f);
//		temp->move = ntohs (temp->move);
    fread (&(temp->value), sizeof (temp->value), 1, f);
//		temp->value = ntohs (temp->value);
    fread (&(temp->flag), sizeof (temp->flag), 1, f);
//		temp->flag = ntohs (temp->flag);
    readbook(&temp->barn,f);
  }
}

/************************************************/
/* readlib konverterer biblioteket til treaform */
/************************************************/

void savebook(booktree * root,FILE * f)
{
  short int children = 0;
  booktree *temp;

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
	fwrite (&children, sizeof (children), 1, f);

	fwrite (&(root->move), sizeof (root->move), 1, f);
	if (root->value == 32600)
		root->value = 0;
	fwrite (&(root->value), sizeof (root->value), 1, f);
	
/*	if (root->flag == C1ALCULATED)
		root->flag = 1;
	else
		root->flag = 0; */
	fwrite (&(root->flag), sizeof (root->flag), 1, f);
  savebook(root->barn,f);

  temp = root;
  while (--children > 0) {
    temp = temp->sosk;
	  fwrite (&(temp->move), sizeof (temp->move), 1, f);
	  fwrite (&(temp->value), sizeof (temp->value), 1, f);
	  fwrite (&(temp->flag), sizeof (temp->flag), 1, f);
    savebook(temp->barn,f);
  }
}



/*******************************************************************************/
/* Get_lib laeser biblioteket ind fra disken, og konverterer det saadan at de  */
/* spejlede versioner bliver dannet                                            */
/*******************************************************************************/

int Get_book(void)
{
  FILE * f;
 
	long book_nodes;
	
	abook = 0;

  if ((f = fopen("opening","rb")) != NULL) {
			fread (&book_nodes, sizeof (book_nodes), 1, f);
		//	book_nodes = ntohl( book_nodes);
			book_nodes += 10000;
			init_book_nodes(book_nodes);
      readbook(&book_root,f);
			fclose (f);
			if (calclib)
				printf("booknodes = %d\n", book_nodes - 10000);
  }
  else
    return FALSE;

  return TRUE;
}

/*******************************************************************************/
/* Get_lib laeser biblioteket ind fra disken, og konverterer det saadan at de  */
/* spejlede versioner bliver dannet                                            */
/*******************************************************************************/

int Put_book(void)
{
  FILE * f;


  if ((f = fopen("opening","wb")) != NULL) {
			fwrite (&abook, sizeof (abook), 1, f);
      savebook(book_root,f);
			fclose (f);
  }
  else
    return FALSE;

  return TRUE;
}

/**************************************************************************/
/* init_nodes initialiserer vores liste af frie knuder. De opbevares i en */
/* hukkommelses blok.                                                     */
/**************************************************************************/

void init_book_nodes(long antal)
{
  booktree * nknude, *sidste;

  nbfree = (struct bnode *)malloc(antal*sizeof(booktree));
  maxbook = antal - 40; 
  /* vi soerger for at der er plads til mindst 40 treak i sidste knude */
  
  abook = 0;

  nknude = nbfree;
  sidste = nbfree + antal - 1;
  /* temp peger nu paa sidste plads i vores blok */
  
  while (nknude < sidste) {
    /* fortsaet indtil slutningen af blokken */
    
    nknude->barn = nknude + 1;
    /* nknude->barn peger nu paa naeste plads */
    
    nknude ++;
  }
  nknude->barn = NULL;
  /* sidste knuder peger ikke paa noget */
}


/***********************************************************************************/
/* put_in_front saetter knuden cur forest i listen der startes med knuden "start"  */
/***********************************************************************************/
void put_bn_in_front(booktree* *start, booktree* *cur)
{
    booktree *temp,*temp1;
    
    if ((temp = *start) == *cur)
      /* hvis den allerede er forest saa retuner */
      return; 
    temp1 = *cur;
    while (temp->sosk != temp1)
      /* ellers finder vi den */
      temp = temp->sosk;
            
    temp->sosk = temp1->sosk;
    /* og fjerner den */
    
    temp1->sosk = (*start); 
    *start = temp1;
    /* og saetter den forest i koen */
    
    *cur = temp;
    /* cur peger nu paa knuden som den vi satte forest i listen pegede paa */
    /* vi bruger nemlig denne til at finde det naeste treak i vores soege- */
    /* algoritme                                                           */
} 

/************************************************************************/
/* de foelgende funktioner er biblioteks funktioner, de afhaenger af    */
/* hvordan, selve biblioteket er defineret, og har derfor forskelligt   */
/* udseende afhaengigt af hvilket bibliotek der benyttes. Her er brugt  */
/* et PD bibliotek fra programmer THOR, og dette er optimeret, saaledes */
/* at treakkene kun bliver lagret for det ene af de foerste 4 mulige    */
/* treak. Dette skyldes, at de foerste 4 treak faktisk er ens, da de    */
/* kan beskrives ud fra hinanden ved spejling om de 2 akser.            */
/************************************************************************/

/************************************************************************/
/* convop bruges til at konvertere et treak til det rigtige tal, hvis   */
/* det foerste treak ikke var det samme som det i biblioteket, men et   */
/* af de spejlvendte treak                                              */
/************************************************************************/

short int convop(short int mov)
{

  short int first,t1,t2,temp;

  first = game.moves[0];
  /* first er nu det foerste treak */
	
	if (mov == 0)
		return 0;

  t1 = mov / 10;
  t2 = mov % 10;
  /* t1 og t2 er nu nummeret paa henholdsvis soejlen og raekken hvor */
  /* det treakket mov blev foretaget                                 */

  /* her spejles treakket mov saadan, at det kommer til at passe med */
  /* de treak der findes i biblioteket                               */
  switch (first) {
    case 34 : temp = mov; break;
    case 43 : temp = t2*10 + t1; break;
    case 56 : temp = (9 - t2)*10 + (9 - t1); break;
    case 65 : temp = (9 - t1)*10 + (9 - t2); break;
  }
  return temp;
}

gamerec transgame,con_game;

short int play_game(board * bd, short int x, gamerec * gam)
{
	short int player,i,j;
	
  for (i = 0; i <= 9; i++) {
    bd->sq[i] = BORDER;
    bd->sq[i + 90] = BORDER;
    bd->sq[10*i] = BORDER;
    bd->sq[10*i +9] = BORDER;
  }

  /* hver spiller har 2 brikker */
  bd->ndiscs[LIGHT] = 2;
  bd->ndiscs[DARK] = 2;

  /* Listen af mulige treak oprettes */
  bd->possible.nmoves = 12;
  bd->possible.move[0] = 33;
  bd->possible.move[1] = 34;
  bd->possible.move[2] = 35;
  bd->possible.move[3] = 36;
  bd->possible.move[4] = 43;
  bd->possible.move[5] = 46;
  bd->possible.move[6] = 53;
  bd->possible.move[7] = 56;
  bd->possible.move[8] = 63;
  bd->possible.move[9] = 64;
  bd->possible.move[10] = 65;
  bd->possible.move[11] = 66;

  /* breadtfelterne saettes til at vaere tomme */
  for (i = 1; i <= 8; i++)
    for (j = 1; j <= 8; j++)
       bd->sq[10*i + j] = EMPTY;

  /* de fire startbrikker saettes pa breaddet */
  bd->sq[44] = LIGHT;
  bd->sq[55] = LIGHT;
  bd->sq[45] = DARK;
  bd->sq[54] = DARK;
	player = DARK;
	for (i = 0; i <= x; i++) {
		if (gam->moves[i] != 0) {
			if (!f_legalmove (player, gam->moves[i], bd))
				return FALSE;
			trymove(gam->moves[i],player, bd);
		}
		player = other(player);
	}
	return TRUE;
}

short int equalbd(board * transbd,board * currentbd)
{
	short int i,j;
	
	for (i = 1; i <= 8; i++)
    for (j = 1; j <= 8; j++)
			if (transbd->sq[10*i + j] != currentbd->sq[10*i + j])
				return FALSE;

	return TRUE;
}

/********************************************************************/
/* getopn vaelger ud fra biblioteket "opn", antallet af treak "bnr" */
/* de tilladte treak "list", det naeste mulige treak i biblioteket  */
/* og retunerer det i "best". Hvis short intet svar treak findes i  */
/* biblioteket retuneres FALSK, ellers retunerers SAND              */
/********************************************************************/

short int transgetopn(booktree * opn, short int bnr,  booktree ** bnode)
{
  short int x,z,t1,t2,mirror;
  

  x = 1;
  mirror = FALSE;
  while (x <= bnr) {
    /* indtil sidste treak i de hidtil udfoerte treak er undersoegt */

    z = convop(transgame.moves[x]);
    /* z er nu den "spejlede" version af treakket i game.moves[x] */

    if ((x > 2) && (convop(transgame.moves[1]) == 33)
         && (convop(transgame.moves[2]) == 43)
         && (convop(transgame.moves[3]) == 53)) {
      /* her er et specialt tilfaelde, hvor en treakraekke foelge */
      /* foerer til en stilling hvor der igen kan udfores 2 treak */
      /* der i praksis er ens, da de efterfoelgende stillinger er */
      /* spejlinger af hinanden                                   */

      t1 = z / 10;
      t2 = z % 10;
      z = t2*10 + t1;
      mirror = TRUE;
    }
    while ((opn->move != z) && (opn->sosk != NULL))
      /* vi leder i biblioteket indtil vi finder treakket, eller */
      /* der ikke er flere treak                                 */
      opn = opn->sosk;

    if (opn->move != z) {
      /* vi kunne ikke finde treakket */
			*bnode = NULL;
      return FALSE;
      /* saa vi slaar biblioteket fra, og retunerer FALSK */
    }
    else {
      opn = opn->barn;
      /* vi ser om der findes et svar paa dette treak i biblioteket */

      if (opn == NULL) {
        /* der var intet svar */

        *bnode = NULL;
        return FALSE;
        /* saa vi slaar biblioteket fra, og retunerer FALSK */
      }
    }
    x++;
    /* vi undersoeger naeste treak */
  }
  *bnode = opn;
   return TRUE;
}

short int getopn(booktree * opn, short int * best, short int bnr, movelist *list, short int * score)
{
	short int x,z,t1,t2,mirror,cmove,leftmoves[20][2],max,bm,found,trans;
	board transbd, currentbd;
  booktree * temp;

  x = 1;
  mirror = FALSE;
	found = TRUE;
  while (x < bnr  && found ) {
    /* indtil sidste treak i de hidtil udfoerte treak er undersoegt */

		transgame.moves[x] = game.moves[x];
    z = convop(game.moves[x]);
    /* z er nu den "spejlede" version af treakket i game.moves[x] */

    if ((x > 2) && (convop(game.moves[1]) == 33)
         && (convop(game.moves[2]) == 43)
         && (convop(game.moves[3]) == 53)) {
      /* her er et specialt tilfaelde, hvor en treakraekke foelge */
      /* foerer til en stilling hvor der igen kan udfores 2 treak */
      /* der i praksis er ens, da de efterfoelgende stillinger er */
      /* spejlinger af hinanden                                   */

      t1 = z / 10;
      t2 = z % 10;
      z = t2*10 + t1;
      mirror = TRUE;
    }
		found = TRUE;
		if (opn != NULL) {
			while ((opn->move != z) && (opn->sosk != NULL))
				/* vi leder i biblioteket indtil vi finder treakket, eller */
				/* der ikke er flere treak                                 */
				opn = opn->sosk;
			if (opn->move != z)
				found = FALSE;
			else {
				opn = opn->barn;
				if (opn == NULL)
					found = FALSE;
			}
		}
		else
			found = FALSE;
			
		trans = FALSE;
		
		if (!found) {
			transgame.moves[x-2] = game.moves[x];
			transgame.moves[x] = game.moves[x - 2];
			found = FALSE;
			if (play_game(&transbd,x,&transgame)) {
				play_game(&currentbd,x,&game);
				if (equalbd(&transbd,&currentbd)) {
					if (transgetopn(book_root,x,&opn)) {
						if (opn != NULL) 
							found = TRUE;
					}
				}
			} 

			if (!found && x < bnr - 1) {
				transgame.moves[x-2] = game.moves[x - 2];
				transgame.moves[x] = game.moves[x];
				transgame.moves[x-1] = game.moves[x + 1];
				transgame.moves[x+1] = game.moves[x - 1];
				if (play_game(&transbd,(short int)(x + 1),&transgame)) {
					play_game(&currentbd,(short int)(x + 1),&game);
					if (equalbd(&transbd,&currentbd)) {
						if (transgetopn(book_root,(short int)(x + 1),&opn)) {
							x++;
							if (opn != NULL) 
								found = TRUE;
						}
					}
				}
			} 
			if (!found && x < bnr - 2) {
				transgame.moves[x-1] = game.moves[x - 1];
				transgame.moves[x+1] = game.moves[x+1];
				transgame.moves[x] = game.moves[x + 2];
				transgame.moves[x+2] = game.moves[x];
				if (play_game(&transbd,(short int)(x+2),&transgame)) {
					play_game(&currentbd,(short int)(x+2),&game);
					if (equalbd(&transbd,&currentbd)) {
						if (transgetopn(book_root,(short int)(x+2),&opn)) {
							x += 2;
							if (opn != NULL) 
								found = TRUE;
						}
					}
				}
			} 
		} 

    x++;
    /* vi undersoeger naeste treak */
  }
	if (!found) {
		libon = FALSE;
		return FALSE;
	}

  x = 0;
  temp = opn;
  /* temp peger nu paa det foerste svartreak, der kan godt vaere mere */
  /* end et muligt svar i biblioteket                                 */

  do {
    /* de naeste linier konverterer treakkene i biblioteket tilbage */
    /* til den rigtige form, altafhaengig af hvor mange gange der   */
    /* er blevet spejlet                                            */
    if (mirror) {
      z = convop(temp->move);
      t1 = z / 10;
      t2 = z % 10;
      cmove = t2*10 + t1;
    }
    else
      cmove = convop(temp->move);

    if (inlist(cmove, list))
      /* cmove er et legalt treak. Det skal nu undersoeges, om dette      */
      /* treak allerede er blevet provet. Dette kunne ske, hvis spilleren */
      /* vaelger menupunkter "nextbest", der faar computeren til at prove */
      /* et andet treak, ved at fjerne det sidst udforte treak fra listen */
      /* af mulige treak "list"                                           */
			leftmoves[x][0] = temp->value;
      leftmoves[x++][1] = cmove;
			
    temp = temp->sosk;
  } while (temp != NULL);
  /* indtil der ikke er flere treak i biblioteket */

  if (x == 0) {
    /* hvis x er 0, saa var der ikke flere treak i biblioteket der ikke */
    /* allerede var proevet                                             */
    libon = FALSE;
    return FALSE;
  }
	
	max = -32665;
	z = 0;
	while (z < x) {
		if (leftmoves[z][0] > max) {
			max = leftmoves[z][0];
			bm = z;
		}
		z++;
	}
	*score = leftmoves[bm][0];
	*best = leftmoves[bm][1]; 
	
	*score = leftmoves[0][0];
	*best = leftmoves[0][1]; 


#if 0
 if (x > 1) {
    /* hvis x > 1, saa vaelger vi et tilfaeldigt af de mulige svar */
		z = rand() % x;
		 *score = leftmoves[z][0];
		 *best = leftmoves[z][1]; 
	}
  else {
    /* ellers vaelges det eneste mulige */
    *best = leftmoves[0][1];
		*score = leftmoves[0][0];
	} 
#endif

  return TRUE;
}

booktree * getopnpos(booktree * opn, short int bnr)
{
	short int x,z,t1,t2,mirror,found,trans;
	board transbd, currentbd;


  x = 1;
  mirror = FALSE;
  while (x < bnr) {
    /* indtil sidste treak i de hidtil udfoerte treak er undersoegt */

		transgame.moves[x] = game.moves[x];
    z = convop(game.moves[x]);
    /* z er nu den "spejlede" version af treakket i game.moves[x] */

    if ((x > 2) && (convop(game.moves[1]) == 33)
         && (convop(game.moves[2]) == 43)
         && (convop(game.moves[3]) == 53)) {
      /* her er et specialt tilfaelde, hvor en treakraekke foelge */
      /* foerer til en stilling hvor der igen kan udfores 2 treak */
      /* der i praksis er ens, da de efterfoelgende stillinger er */
      /* spejlinger af hinanden                                   */

      t1 = z / 10;
      t2 = z % 10;
      z = t2*10 + t1;
      mirror = TRUE;
    }
		found = TRUE;
		if (opn != NULL) {
			while ((opn->move != z) && (opn->sosk != NULL))
				/* vi leder i biblioteket indtil vi finder treakket, eller */
				/* der ikke er flere treak                                 */
				opn = opn->sosk;
			if (opn->move != z)
				found = FALSE;
			else {
				opn = opn->barn;
				if (opn == NULL)
					found = FALSE;
			}
		}
		else
			found = FALSE;
			
		trans = FALSE;
		
		if (!found) {
			transgame.moves[x-2] = game.moves[x];
			transgame.moves[x] = game.moves[x - 2];
			found = FALSE;
			if (play_game(&transbd,x,&transgame)) {
				play_game(&currentbd,x,&game);
				if (equalbd(&transbd,&currentbd)) {
					if (transgetopn(book_root,x,&opn)) {
						if (opn != NULL) 
							found = TRUE;
					}
				}
			} 
			if (!found && x < bnr - 1) {
				transgame.moves[x-2] = game.moves[x - 2];
				transgame.moves[x] = game.moves[x];
				transgame.moves[x-1] = game.moves[x + 1];
				transgame.moves[x+1] = game.moves[x - 1];
				if (play_game(&transbd,(short int)(x + 1),&transgame)) {
					play_game(&currentbd,(short int)(x + 1),&game);
					if (equalbd(&transbd,&currentbd)) {
						if (transgetopn(book_root,(short int)(x + 1),&opn)) {
							x++;
							if (opn != NULL) 
								found = TRUE;
						}
					}
				}
			} 
			if (!found && x < bnr - 2) {
				transgame.moves[x-1] = game.moves[x - 1];
				transgame.moves[x+1] = game.moves[x+1];
				transgame.moves[x] = game.moves[x + 2];
				transgame.moves[x+2] = game.moves[x];
				if (play_game(&transbd,(short int)(x+2),&transgame)) {
					play_game(&currentbd,(short int)(x+2),&game);
					if (equalbd(&transbd,&currentbd)) {
						if (transgetopn(book_root,(short int)(x+2),&opn)) {
							x += 2;
							if (opn != NULL) 
								found = TRUE;
						}
					}
				}
			} 

		}

    x++;
    /* vi undersoeger naeste treak */
  }
	if (!found) 
		return NULL;


  return opn;
}

/********************************************************************/
/* blackstart er de fire foerste mulige treak, bruges naar brugeren */
/* vaelger menupunkt "nextbest" i short interfacet.                       */
/********************************************************************/

short int blackstart[4] = {34,43,56,65};

/*************************************************************************/
/* getlib undersoeger, om spilleren "pl", har et svartreak i biblioteket */
/*************************************************************************/

short int getlib(short int pl, short int * bstmov, short int movenum, movelist *list, short int *score)
{
  short int leftmoves[4],x,y;
	
	for (x = 0; x <= 60; x++)
		transgame.moves[x] = 0;
	
	transgame.moves[0] = game.moves[0];


  if (pl == DARK) {
    /* hvis spilleren er sort, saa skal stillingen lige behandles   */
    /* specialt. Dette skyldes, at biblioteket gaar ud fra et       */
    /* starttraek, og ikke gemmer dette i biblioteket. Vi behandler */
    /* derfor denne stilling specialt her                           */
    if (movenum == 0) {
      x = 0;
      for (y = 0; y <= 3; y++)
        if (inlist(blackstart[y], list))
          leftmoves[x++] = blackstart[y];
          /* leftmoves indeholder nu de starttreak der ikke er blevet */
          /* proevet allerede.                                        */

      *bstmov = leftmoves[rand() % x];
			*score = 0;
      return TRUE;
    }
    else
      return getopn(book_root,bstmov,movenum,list, score);
  }
  else
    return getopn(book_root,bstmov,movenum,list, score);
}

booktree * getlibpos(short int movenum)
{
  short int x;
	
	for (x = 0; x <= 60; x++)
		transgame.moves[x] = 0;
	
	transgame.moves[0] = game.moves[0];

    return getopnpos(book_root,movenum);
}


void convert_game(void)
{
	short int x,z,t1,t2,player;
	
	con_game.moves[0] = 34;
	con_game.sidste = game.sidste;
	x = 1;
	
  while (x <= game.sidste) {
    /* indtil sidste treak i de hidtil udfoerte treak er undersoegt */

    z = convop(game.moves[x]);
    /* z er nu den "spejlede" version af treakket i game.moves[x] */

    if ((x > 2) && (convop(game.moves[1]) == 33)
         && (convop(game.moves[2]) == 43)
         && (convop(game.moves[3]) == 53)) {
      /* her er et specialt tilfaelde, hvor en treakraekke foelge */
      /* foerer til en stilling hvor der igen kan udfores 2 treak */
      /* der i praksis er ens, da de efterfoelgende stillinger er */
      /* spejlinger af hinanden                                   */
			con_game.moves[3] = 35;

      t1 = z / 10;
      t2 = z % 10;
      z = t2*10 + t1;
    }
		con_game.moves[x] = (char)z;
		x++;
	}
			calc = FALSE;
			bcalc = FALSE;
			init_game();
			playnm = game.sidste = con_game.sidste;
			player = DARK;
			for (x = 0; x <= playnm; x++) {
				game.moves[x] = con_game.moves[x];
				game.boards[x] = mainboard;
				if (con_game.moves[x] != 0)
					trymove(con_game.moves[x],player,&mainboard);
				player = other(player);
			}
			mainboard = game.boards[playnm];
			if (playnm % 2)
				human = LIGHT;
			else
				human = DARK;
			computer = other(human);
}

int test(short int x)
{
	return x++;
}

void test1(void)
{
	short int x;
	test(x + 1);
}

void mmgame(short int depth,
						short int win,
							booktree ** root)
{
  booktree *temp;

	if (depth >= game.sidste || depth > 56)
		return;
		
  if (*root == NULL) {
		make_book_node(root);
		(*root)->move = game.moves[depth];
		(*root)->value = win;
		if ((temp = getlibpos(depth + 1)) != NULL)
			mmgame(depth + 1,-win,&temp);
		else 
			mmgame(depth + 1,-win,&(*root)->barn);
	}
	else {
		if ((temp = getlibpos(depth + 1)) != NULL)
			mmgame(depth + 1,-win,&temp);
		else {
			temp = *root;
			while (temp->sosk != NULL && temp->move != game.moves[depth])
				temp = temp->sosk;
			if (temp->move != game.moves[depth]) {
				make_book_node(&temp->sosk);
				temp = temp->sosk;
				temp->move = game.moves[depth];
				temp->value = win;
			}
			mmgame(depth + 1,-win,&temp->barn);
		}
	}
}


void clear_hash(void);

short int getvalue(short int player,movelist *newlist,board * bd,short int *junk)
{

	/* *junk = newlist->move[0];
	return -1; */
	
	computer = player;
  human = other(computer);
	libon = FALSE;
	tryagain = 5;
	delres(&comres[0][0]);
	delres(&humres[0][0]); 
	clear_hash();


	calc = FALSE;
	b1calc = bcalc = FALSE;
	mainboard = * bd;
	list = *newlist;
	if (m_pStelloView != NULL)
		m_pStelloView->RedrawWindow();
	*junk = getcomputer();
	if (bcalc) {
		if (bvalue > 0)
  		return bvalue + 32600;
		else
			return bvalue - 32600;
	}
	else if(b1calc) {
		if (bvalue > 0)
  		return bvalue + 32600;
		else
			return bvalue - 32600;
	}
	else
	  return value;
}

void delmove(short int k, movelist *list);

void remove_oldmov(movelist * list,booktree* stree) 
{
	while (stree != NULL) {
		delmove(stree->move,list);
		stree = stree->sosk;
	}
}

short int book_timeout(void)
{
	time_t t;
	struct tm * tid;
	
	 return FALSE; 
	
  t = time(NULL);
	tid = localtime(&t);
	
	if (tid->tm_hour >= 7 && tid->tm_min >= 30)
		return TRUE;
	else
		return FALSE;
}


/**********************************************************************/
/* minmaxlib beregner scorer til alle knuder i et givet bibliotek     */
/**********************************************************************/

short int minmaxlib(short int player,
						short int depth,
            board *bd,
            booktree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int maxscore,score,trek,junk, opp,mustadd,i;
  booktree *temp, *temp1;

  aknud++;
  temp = (*spiltree);
	opp = other(player);

	maxscore = -32767;
	

  if (temp->move != 0) {
    /* hvis der er lovlige traek */
		mustadd = TRUE;
		do {
			if (temp->flag & CALCULATED) {
				mustadd = FALSE;
				break;
			}
			temp = temp->sosk;
		}
		while (temp != NULL); 
		
		temp = (*spiltree);
		i = 1;
    do {
      trek = temp->move;
      /* naeste mulige treak */

      newbd = *bd;
			if (!f_legalmove (player, trek, bd)) {
				printf("illegalt treak %d, nr %d\n", trek,i);
				if (temp == (*spiltree)) {
				  if (temp->sosk == NULL) {
						(*spiltree) = NULL;
						return 0;
					}
					else {
						temp = temp->sosk;
						(*spiltree) = temp;
						continue;
					}
				}
				else {
					temp1 = (*spiltree);
					while (temp1->sosk != temp)
						temp1 = temp1->sosk;
					temp1->sosk = temp->sosk;
					temp = temp1->sosk;
					if (temp == NULL)
						return 0;
					else
						continue;
				}
				goto NEXT;
			}
      trymove(trek,player,&newbd);
			game.moves[depth] = (char)trek;

			/* hvis vi er ved en knude, hvis barn findes i det gemte trae */
			/* saa kaldes findmin                                         */
			if (temp->barn != NULL) {
				score = -minmaxlib(opp,depth + 1,&newbd,&temp->barn);		
				temp->flag &= !CALCULATED; 
			}
			else {
				if (!(temp->flag & CALCULATED)) {
					makelist(&newlist,opp,&newbd);
					/* hvis barnet ikke findes i vores trae, undersoeges om der er   */
					/* mere plads, og hvis der er det kaldes findmin1, ellers kaldes */
					/* findmin2                                                      */
					if (getlib(opp,&bestmove,depth + 1,&newlist,&score)) {
						score = -score;
						temp->flag &= !CALCULATED;
					}
					else {
						score =	-getvalue(opp,&newlist,&newbd,&junk);
						temp->flag |= CALCULATED;  
						if (bcalc)
							temp->flag |= EXACT;
						else if (b1calc)
							temp->flag |= INEXACT;
						libmoves ++;
						if (libmoves % 10 == 0) {
							//printf("Libmoves = %d\n", libmoves);
							Put_book();
						} 
					}
				}
				else
					score = temp->value;
			}
			if (score > maxscore) 
        maxscore = score;
			temp->value = score;
			NEXT:;
      temp = temp->sosk;
      /* naeste treak */

      if (book_timeout())
        /* her springer vi ud fra soegningen, hvis tiden er overskredet */
        /* for meget.                                                   */
        longjmp(libenv, 1);
			i++;
    }
    while ((temp != NULL));
    /* indtil ikke flere treak */
		
		if (mustadd) {
			makelist(&newlist,player,bd);
			temp = (*spiltree);
			remove_oldmov(&newlist, temp);  
			if (newlist.nmoves > 0) {
				temp = (*spiltree);
				while (temp->sosk != NULL)
					temp = temp->sosk;
				make_book_node(&temp->sosk);
				temp = temp->sosk;
	
				score = getvalue(player,&newlist,bd,&junk); 
				if (bcalc)
					temp->flag |= EXACT;
				else if (b1calc)
					temp->flag |= INEXACT;
				if (score > maxscore) 
        	maxscore = score;
				temp->move = junk;
				temp->value = score;
				temp->flag |= CALCULATED; 
				libmoves ++;
				if (libmoves % 10 == 0) {
				//	printf("Libmoves = %d\n", libmoves);
					Put_book();
				} 

				abook++;
			}
		}

  }
  else { /* intet lovligt treak */
		game.moves[depth] = 0;
    if (temp->barn != NULL) {
      /* hvis der er et barn, kaldes findmin */
      maxscore = -minmaxlib(opp,(short int)(depth + 1),bd,&temp->barn);
			temp->flag &= !CALCULATED;
		}
    else {
			if (!(temp->flag & CALCULATED)) {
				if (makelist(&newlist, opp, bd) == 0) {
					aeval++;
					/* modstanderen har heller ingen lovlige treak, saa */
					/* spillet er slut                                  */
					maxscore = (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
					if (maxscore < 0)
						maxscore -= 32600;
					else if (maxscore > 0)
						maxscore += 32600;
				}
				else {
					maxscore = -getvalue(opp,&newlist,bd,&junk);
					if (bcalc)
						temp->flag |= EXACT;
					else if (b1calc)
						temp->flag |= INEXACT;
				}
			  temp->flag |= CALCULATED;  
				libmoves ++;
				if (libmoves % 10 == 0) {
				//	printf("Libmoves = %d\n", libmoves);
					Put_book();
				} 
			}
			else
				maxscore = temp->value;
    }
		temp->value = maxscore;
  }
  return maxscore;
}

void sort_nodes(booktree* *start,short int nodes)
{
    booktree *temp,*bnodes[40],dummy;
		short int i,j;
		
		if (nodes < 2)
			return;
    
		i = 0;
		temp = *start;
		
		while (temp != NULL) {
			bnodes[i++] = temp;
			temp = temp->sosk;
		}
		
		dummy.value = -32666;
		bnodes[nodes] = &dummy;
		for (i = nodes-2; i>=0; i--) {
			temp = bnodes[i];
			for (j=i+1; temp->value < bnodes[j]->value; j++)
				bnodes[j-1] = bnodes[j];
			bnodes[j-1] = temp;
		}
		
		for (i = 0; i < nodes - 1; i++)
			bnodes[i]->sosk = bnodes[i+1];
		bnodes[nodes - 1]->sosk = NULL;
		*start = bnodes[0];
 } 

void sort_lib(booktree ** root)
{
  short int children = 0;
  booktree *temp;

  if (!*root) 
    return;
 
	temp = *root;
	while (temp->sosk != NULL) {
		temp = temp->sosk;
		children ++;
	}
	children ++;
	
	sort_nodes(root, children);
	temp = *root;
  sort_lib(&temp->barn);

  temp = *root;
  while (--children > 0) {
    temp = temp->sosk;
    sort_lib(&temp->barn);
  }
}


short int extendlib(short int player,
						short int look,
						short int depth,
            board *bd,
            booktree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int maxscore,score,trek,junk, opp,mustadd,addchild,i;
  booktree *temp,*temp1;

  aknud++;
 	
	if (*spiltree == NULL) {
		make_book_node(spiltree);
		temp = (*spiltree);
		newbd = *bd;
		makelist(&newlist,player, &newbd);
		score = getvalue(player,&newlist, &newbd,&junk); 
		if (bcalc)
			temp->flag |= EXACT;
		else if (b1calc)
			temp->flag |= INEXACT;

		temp->move = junk;
		temp->value = score;
		temp->flag |= CALCULATED; 
		libmoves ++;
		if (libmoves % 10 == 0) {
			printf("Libmoves = %d\n", libmoves);
			Put_book();
		}

		abook++;
	}

	temp = (*spiltree);

	opp = other(player);

	maxscore = -32767;
	

  if (temp->move != 0) {
    /* hvis der er lovlige traek */
		if (temp->barn == NULL && look > 0)
			addchild = TRUE;
		else
			addchild = FALSE;
			
		mustadd = TRUE;
		if (addchild)
			temp = temp->sosk;
		if (temp != NULL) {
			do {
				if (temp->flag & CALCULATED) {
					mustadd = FALSE;
					break;
				}
				temp = temp->sosk;
			}
			while (temp != NULL); 
		}
		
		temp = (*spiltree);


		i = 1;
    do {
      trek = temp->move;
      /* naeste mulige treak */

      newbd = *bd;
			if (!f_legalmove (player, trek, bd)) {
				printf("illegalt treak %d, nr %d\n", trek,i);
				if (temp == (*spiltree)) {
				  if (temp->sosk == NULL) {
						(*spiltree) = NULL;
						return 0;
					}
					else {
						temp = temp->sosk;
						(*spiltree) = temp;
						continue;
					}
				}
				else {
					temp1 = (*spiltree);
					while (temp1->sosk != temp)
						temp1 = temp1->sosk;
					temp1->sosk = temp->sosk;
					temp = temp1->sosk;
					if (temp == NULL)
						return 0;
					else
						continue;
				}
				goto NEXT;
			}
      trymove(trek,player,&newbd);
			game.moves[depth] = (char)trek;

			/* hvis vi er ved en knude, hvis barn findes i det gemte trae */
			/* saa kaldes findmin                                         */

			if (temp->barn != NULL || (addchild && i == 1)) {
				if (temp->barn == NULL) {
					makelist(&newlist,opp,&newbd);
					if (getlib(opp,&bestmove, depth + 1,&newlist,&score))
						score = -score;
					else {
						if (abs(temp->value) < 150) {
							if (i > 1) {
								if (look > lowlook)
									score = -extendlib(opp,look - 1,depth + 1,&newbd,&temp->barn);
								else
									score = temp->value;
							}
							else 
								score = -extendlib(opp,look - 1,depth + 1,&newbd,&temp->barn);
						}
						else
							score = temp->value;
					}
				}
				else {
					if (abs(temp->value) < 150) {
						if (i > 1) {
							if (look > lowlook)
								score = -extendlib(opp,look - 1,depth + 1,&newbd,&temp->barn);
							else
								score = temp->value;
						}
						else 
							score = -extendlib(opp,look - 1, depth + 1,&newbd,&temp->barn);
					}
					else
						score = temp->value;
				}	
				temp->flag &= !CALCULATED; 
			}
			else {
				if (!(temp->flag & CALCULATED)) {
					makelist(&newlist,opp,&newbd);
					/* hvis barnet ikke findes i vores trae, undersoeges om der er   */
					/* mere plads, og hvis der er det kaldes findmin1, ellers kaldes */
					/* findmin2                                                      */
					if (getlib(opp,&bestmove,depth + 1,&newlist,&score)) {
						score = -score;
						temp->flag &= !CALCULATED;
					}
					else {
						score =	-getvalue(opp,&newlist,&newbd,&junk);
						if (bcalc)
							temp->flag |= EXACT;
						else if (b1calc)
							temp->flag |= INEXACT;

						temp->flag |= CALCULATED;  
						libmoves ++;
						if (libmoves % 10 == 0) {
							printf("Libmoves = %d\n", libmoves);
							Put_book();
						}
					}
				}
				else
					score = temp->value;
			}
			if (score > maxscore) 
        maxscore = score;
			temp->value = score;
			NEXT:;
      temp = temp->sosk;
      /* naeste treak */

      if (book_timeout())
        /* her springer vi ud fra soegningen, hvis tiden er overskredet */
        /* for meget.                                                   */
        longjmp(libenv, 1);
			i++;
    }
    while ((temp != NULL));
    /* indtil ikke flere treak */
		
		if (mustadd) {
			makelist(&newlist,player,bd);
			temp = (*spiltree);
			remove_oldmov(&newlist, temp);  
			if (newlist.nmoves > 0) {
				temp = (*spiltree);
				while (temp->sosk != NULL)
					temp = temp->sosk;
				make_book_node(&temp->sosk);
				temp = temp->sosk;
	
				score = getvalue(player,&newlist,bd,&junk); 
				if (bcalc)
					temp->flag |= EXACT;
				else if (b1calc)
					temp->flag |= INEXACT;

				if (score > maxscore) 
        	maxscore = score;
				temp->move = junk;
				temp->value = score;
				temp->flag |= CALCULATED; 
				libmoves ++;
				if (libmoves % 10 == 0) {
					printf("Libmoves = %d\n", libmoves);
					Put_book();
				}

				abook++;
			}
		}

  }
  else { /* intet lovligt treak */
		game.moves[depth] = 0;
    if (temp->barn != NULL) {
      /* hvis der er et barn, kaldes findmin */
      maxscore = -extendlib(opp,look,(short int)(depth + 1),bd,&temp->barn);
			temp->flag &= !CALCULATED;
		}
    else {
			if (!(temp->flag & CALCULATED)) {
				if (makelist(&newlist, opp, bd) == 0) {
					aeval++;
					/* modstanderen har heller ingen lovlige treak, saa */
					/* spillet er slut                                  */
					maxscore = (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
					if (maxscore < 0)
						maxscore -= 32600;
					else if (maxscore > 0)
						maxscore += 32600;
					temp->flag |= EXACT;
				}
				else {
					maxscore = -getvalue(opp,&newlist,bd,&junk);
					if (bcalc)
						temp->flag |= EXACT;
					else if (b1calc)
						temp->flag |= INEXACT;
				}
			  temp->flag |= CALCULATED;  
				libmoves ++;
				if (libmoves % 10 == 0) {
					printf("Libmoves = %d\n", libmoves);
					Put_book();
				}
			}
			else
				maxscore = temp->value;
    }
		temp->value = maxscore;
  }
  return maxscore;
}


short int mmlib(short int player,
            short int depth,
            board *bd,
            booktree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int maxscore,score,trek, opp,i,bestmove;
  booktree *temp,*temp1;

  aknud++;
  temp = (*spiltree);
	opp = other(player);

	maxscore = -32767;
	

  if (temp->move != 0) {
    /* hvis der er lovlige traek */
		
		i = 1;
    do {
      trek = temp->move;
      /* naeste mulige treak */

      newbd = *bd;
			if (!f_legalmove (player, trek, bd)) {
				printf("illegalt treak %d, nr %d\n", trek,i);
				if (temp == (*spiltree)) {
				  if (temp->sosk == NULL) {
						(*spiltree) = NULL;
						return 0;
					}
					else {
						temp = temp->sosk;
						(*spiltree) = temp;
						continue;
					}
				}
				else {
					temp1 = (*spiltree);
					while (temp1->sosk != temp)
						temp1 = temp1->sosk;
					temp1->sosk = temp->sosk;
					temp = temp1->sosk;
					if (temp == NULL)
						return 0;
					else
						continue;
				}
				goto NEXT;
			}
      trymove(trek,player,&newbd);
			game.moves[depth] = (char)trek;

			/* hvis vi er ved en knude, hvis barn findes i det gemte trae */
			/* saa kaldes findmin                                         */
			if (temp->barn != NULL) 
				score = -mmlib(opp,depth + 1,&newbd,&temp->barn);		
			else {
				makelist(&newlist,opp,&newbd);
				/* if ((temp1 = getlibpos(depth + 1)) != NULL)
					score = -mmlib(opp,depth + 1,&newbd,&temp1); */
				if (getlib(opp,&bestmove,depth + 1,&newlist,&score))
					score = -score; 
				else 
					score = temp->value;
			}
				
			if (score > maxscore) 
        maxscore = score;
			temp->value = score;
			NEXT:;
      temp = temp->sosk;
      /* naeste treak */

			i++;
    }
    while ((temp != NULL));
    /* indtil ikke flere treak */
		

  }
  else { /* intet lovligt treak */
		game.moves[depth] = 0;
    if (temp->barn != NULL) 
      maxscore = -mmlib(opp,depth + 1,bd,&temp->barn);
    else {
			makelist(&newlist,opp,bd);
			if (getlib(opp,&bestmove,depth + 1,&newlist,&maxscore))
				maxscore = -maxscore;
			else
				maxscore = temp->value;
		}
 		temp->value = maxscore;
  }
  return maxscore;
}


void calc_lib(void)
{
	board newbd;
	libmoves = 0;
	
	if (setjmp(libenv)) {
		return;
	}
	
	tid_kontrol = tid_per_trek;
	lookahead = LIBLEVEL;
	
	game.moves[0] = 34;
	play_game(&newbd,0,&game);

	
	minmaxlib(LIGHT,1,&newbd,&book_root); 


//	printf("Libmoves = %d\n", libmoves);
}

void minmax_lib(void)
{
	board newbd;
	
	

			
	game.moves[0] = 34;
	play_game(&newbd,0,&game);
	
	mmlib(LIGHT,1,&newbd,&book_root); 


}

void extend_lib(void)
{
	FILE * f;
	board newbd;
	int look1,look2,totalnodes,x;
	
	libmoves = 0;
	
	if (setjmp(libenv)) {
		return;
	}
	
	system("date >> expanding");
	tid_kontrol = tid_per_trek;
	lookahead = LIBLEVEL;
	
	for (x = 0; x < 10; x++)
		minmax_lib();
	sort_lib(&book_root);


	
	game.moves[0] = 34;
	play_game(&newbd,0,&game);
	
	look1 = 5;
	do {
		look2 = 4;
		totalnodes = 0;
		printf("BDepth = %d\n", look1);
		do {
			do {
				libmoves = 0;
				lowlook = look2/2;
				printf("SDepth = %d\n", look2);
				extendlib(LIGHT,look2,1,&newbd,&book_root); 
				for (x = 0; x < 10; x++)
					minmax_lib();
				sort_lib(&book_root);
				printf("All Libmoves = %d\n", libmoves);
				totalnodes += libmoves;
				if (libmoves != 0) {
					if ((f = fopen("expanding","a")) != NULL) {
						fprintf(f,"%d libmoves at depth %d\n", libmoves, look2);
						fclose (f);
					}
					Put_book();
					look2 = 4;
				}
			} while (libmoves != 0);
			look2++;
		} while (look2 < look1);
		printf("%d libmoves at depth %d\n", totalnodes,look1);
		if ((f = fopen("expanding","a")) != NULL) {
      fprintf(f,"%d libmoves finished at depth %d\n", totalnodes,look1);
			fclose (f);
  	}
		look1++;
	}while (look1 < 18);

	for (x = 0; x < 10; x++)
		minmax_lib();
	sort_lib(&book_root);
	printf("Libmoves = %d\n", libmoves);
}


void splay(void)
{
	short int trek;

		init_game();
		if (libok) {
      libon = TRUE;
			tryagain = 0;
		}
		calc = FALSE;
		bcalc = FALSE;
		
	do {
		calc = FALSE;
		bcalc = FALSE;
  	computer = human;
  	human = other(human);


		if (makelist(&list, computer, &mainboard)) {
			trek = getcomputer();
			makemove(trek, computer);
			if ((makelist(&list, computer, &mainboard) +
					makelist(&list, human, &mainboard)) == 0)
				gameover = TRUE;
		}
		else { /* computer kan ikke trkke */
			if (makelist(&list, human, &mainboard)) {
				calc = FALSE;
				bcalc = FALSE;
				game.moves[playnm++] = 0;
				game.sidste = playnm;
				game.boards[playnm] = mainboard;
			}
			else
				gameover = TRUE;		
		}	
		if (m_pStelloView != NULL)
			m_pStelloView->RedrawWindow();
	}	while (!bcalc && !gameover && !book_timeout());
	
	convert_game();
	if (computer == DARK) {
		if (value > 0)
			mmgame(1,-32665, &book_root); 
		else
			mmgame(1,32665, &book_root);
	}
	else {
		if (value > 0)
			mmgame(1,32665, &book_root); 
		else
			mmgame(1,-32665, &book_root);
	}
}

void selfplay(void)
{
	FILE * f;
	short int x,games;
	
		libmoves = 0;
	
	
	//system("date >> selfplay");
	tid_kontrol = tid_per_trek;
	lookahead = LIBLEVEL;
	
	calc_lib();
	if (book_timeout())
		return;
	calc_lib();
	if (book_timeout())
		return;

	for (x = 0; x < 10; x++)
		minmax_lib();
	sort_lib(&book_root);
	Put_book();
	
	games = 0;

	while (!book_timeout()) {
		splay();
		games ++;
		Put_book();
		if ((f = fopen("selfplay","a")) != NULL) {
      fprintf(f,"played game %d\n",games);
			fclose (f);
  	}

		if (book_timeout())
			return;
		calc_lib();
		if (book_timeout())
			return;
		calc_lib();
		if (book_timeout())
			return;
		for (x = 0; x < 10; x++)
			minmax_lib();
		sort_lib(&book_root);
		Put_book();
	}
}

