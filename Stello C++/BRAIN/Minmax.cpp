/**************************************************************************/
/*                                                                        */
/*     minmax.c, de forskellige versioner af alfa-beta algoritmen         */
/*                                                                        */
/**************************************************************************/
#include "stdafx.h"
#include "stddef.h"
#include <stdlib.h>
#include <setjmp.h>
#include <time.h>
#include "reversi.h"
#include "hash.h"




/* dette er forskellige makroer, der kan saettes til forskellige vaerider */
/* for at teste bestemte algoritmer, hvis RESPON saettes til 0, saa       */
/* bruges response killer algoritmen ikke, hvis NEWKIL saettes til 0 saa  */
/* buges killer algoritmen ikke. ALFA og BETA kan saettes til -32600 og   */
/* 32600 for at undersoege, hvor meget det giver, at teste for alfa-beta  */
/* vinduet i vores evalueringsfunktion hvor der sparer en del tid, ved    */
/* at undlade at beregne mobiliteten hvis det ikke er noedvendigt.        */
/* Endelig er der en makro der hedder NOEXTEN, hvis den defineres vil     */
/* den selektive uddybning af bestemte stillinger slaas fra. Dette sker   */
/* bedst ved at definere den i komando linien, da den ogsaa bruges i      */
/* evalueringsfunktionen                                                  */

#define RESPON trek
#define NEWKIL newkil
#define ALFA alfa
#define BETA beta
#define E_BOUND 50

#define PUT_HASH_HEIGHT (hash_put_height)
#define GET_HASH_HEIGHT (hash_get_height) 
#define PUT_END_HASH_HEIGHT (end_hash)
#define GET_END_HASH_HEIGHT (end_hash)
#define REHASH 3
#define SELEXT 0

#define SAVE_DEPTH 8

#define IF_HASH_GET(H)    if (H <= GET_HASH_HEIGHT)
#define IF_HASH_PUT(H)    if (H <= PUT_HASH_HEIGHT)
#define IF_END_HASH_GET(H)    if (H <= GET_END_HASH_HEIGHT)
#define IF_END_HASH_PUT(H)    if (H <= PUT_END_HASH_HEIGHT)



#define DP 3
#define D1 7
#define D2 5


#define KANTER 6561     /* 3 i 8 forskellige kanter */
#define FARLIG_HJ1_SORT 0x80
#define FARLIG_HJ2_SORT 0x40
#define FARLIG_HJ1_HVID 0x20
#define FARLIG_HJ2_HVID 0x10

char f_legalmove(short int pl, short int trek, board *bd);

extern short int dirs [89][16],low_min, tid_udlobet,backthink;

extern jmp_buf env;

int humres[78][78],comres[78][78];
extern short int hjo_trek[KANTER];
extern long search_time, gam_tid;
extern int hash_get_height,hash_put_height,end_hash;
short int computer,human,treeon,proctid;
tree *root,*nfree,*fnode,*memslut;
long aeval,aknud,aalloc,maxalloc;
short int sindex1,sindex2,sindex3,sindex4;
extern struct rusage timeused;
int getrusage(int who, struct rusage * rusage);


/**********************************************************************/
/* timeout kaldes hvergang et treak er undersoegt for at undersoege   */
/* om den tilladte tid er opbrugt. Naar en soegning foerst er startet */
/* saa tillades det at bruge op til 30 % mere tid en foerst tildelt   */
/**********************************************************************/

short int timeout(void)
{
  long ny_tid;
        
	ny_tid = clock(); 


  if (tid_kontrol == sogedybde)
    return FALSE;
  /* vi soeger her til en bestemt dybde, saa alle treak paa et niveau */
  /* bliver undersoegt                                                */
  else if (tid_kontrol == tid_per_trek) {
		if (backthink)
			return FALSE;
		else
    	return ((ny_tid - gam_tid)*10 > rtider[lookahead]*13);
  /* vi tillader, at tiden overskrides med op til 30 % midt i en */
  /* soegning                                                    */
	}
  else {
		if (backthink)
			return FALSE;
		else
    	return ((ny_tid - gam_tid)*10 > timemove*13);
  /* vi tillader, at tiden overskrides med op til 30 % midt i en */
  /* soegning                                                    */
	}
}

/**************************************************/
/* delres, sletter alle respons killer vaerdierne */
/**************************************************/

void delres(int *res)
{
  short int x;
  x = 337;
  do {
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
    *res++ = 0;
  }
  while (--x >= 0);
}

/* void put_in_front(tree* *start, tree* *cur)
{
  tree *temp,*temp1;

  if ((temp = *start) == *cur)
    return;
  temp1 = *cur;
  while (temp->sosk != temp1)
    temp = temp->sosk;
  temp->sosk = temp1->sosk;
  temp1->sosk = (*start);
  *start = temp1;
  *cur = temp;
}

void put_at_p2(tree* *start, tree* *cur)
{
  tree *temp,*temp1;

  temp = *start;
  temp1 = *cur;

  while (temp->sosk != temp1)
    temp = temp->sosk;
  temp->sosk = temp1->sosk;

  temp1->sosk = (*start)->sosk;
  (*start)->sosk = temp1;
}


void list_in_tree(movelist *list,tree* *spiltree)
{
  long x,*temp;
  char *flis;
  tree *ftree = nfree;

  flis = (char *)list;
  x = *flis++;
  aalloc += x--;
  if (aalloc > maxalloc)
    treeon = FALSE;
  (*spiltree) = ftree;
  do  {
    temp = (long *)ftree;
    ftree = ftree->barn;
    *temp++ = (long)NULL;
    *temp++ = (long)ftree;
    *(short int *)temp = (short int)*flis++;
  }
  while (--x >= 0);
  (*(temp - 1)) = (long)NULL;
  nfree = ftree;
}


void make_node(tree* *spiltree)
{
  long *temp;

  (*spiltree) = nfree;
  temp = (long*)nfree;
  nfree = (tree *)*temp;
  if (++aalloc > maxalloc)
    treeon = FALSE;
  *temp++ = 0;
  *temp++ = 0;
  *(short int *)temp = 0; 
}

short int side_index[100] = {0,0,0,0,0,0,0,0,0,0,
                       0,5,1,1,1,1,1,1,6,0,
                       0,4,0,0,0,0,0,0,2,0,
                       0,4,0,0,0,0,0,0,2,0,
                       0,4,0,0,0,0,0,0,2,0,
                       0,4,0,0,0,0,0,0,2,0,
                       0,4,0,0,0,0,0,0,2,0,
                       0,4,0,0,0,0,0,0,2,0,
                       0,8,3,3,3,3,3,3,5,0,
                       0,0,0,0,0,0,0,0,0,0};

void update_index(short int move,short int * indexex)
{
} */

void put_at_p2(tree* *start, tree* *cur)
{
  tree *temp,*temp1;

  temp = *start;
  temp1 = *cur;

  while (temp->sosk != temp1)
    temp = temp->sosk;
  temp->sosk = temp1->sosk;

  temp1->sosk = (*start)->sosk;
  (*start)->sosk = temp1;
}

hash_square H_B[100];
int c_hash_no;
int c_hash_no1;
long HashCall, HashHit, TransHit;


t_hentry *v_hentry = 0;


void clear_hash(void) 
{
	int i;
	
  for (i = 0; i < c_hash_no; i++) {
    v_hentry[i].f = XX_HASH;
    v_hentry[i].d = 0;
    v_hentry[i].yx = 0;
    v_hentry[i].v = 0;
    v_hentry[i].a1 = 0;
  }

}

void f_hash_init (void)
{
  register long i,j,z;

  c_hash_no = (1 << HASHSIZE);
  c_hash_no1 = c_hash_no - 1;

  v_hentry = (t_hentry *)malloc (sizeof (v_hentry[0]) * c_hash_no + REHASH);

  srand (0x1234567);
  srand (rand ());
  srand (rand ());
  srand (rand ());

	H_B[0].a0 =
		(H_B[0].a0p2 = rand ()) ^
		(H_B[0].a0p1 = rand ());
	H_B[0].a1 =
		(H_B[0].a1p1 = rand ()) ^
		(H_B[0].a1p2 = rand ());

	
	for (i = 10; i <= 80; i += 10)
    for (j = 1; j <= 8; j++) {
			z = i + j;
			H_B[z].a0 =
				(H_B[z].a0p2 = rand ()) ^
				(H_B[z].a0p1 = rand ());
			H_B[z].a1 =
				(H_B[z].a1p1 = rand ()) ^
				(H_B[z].a1p2 = rand ());
  }

  for (i = 0; i < c_hash_no; i++) {
    v_hentry[i].f = XX_HASH;
    v_hentry[i].d = 0;
    v_hentry[i].yx = 0;
    v_hentry[i].v = 0;
    v_hentry[i].a1 = 0;
  }

}

#ifdef __GNUC__ 
inline 
#endif 
void 
f_hash_put (char d, char f, short int v, short int yx, hash_num hash_numbers)
{
  register t_hentry *h = &(v_hentry[m_hash_adr (hash_numbers.a0)]);

  if (h->a1 == hash_numbers.a1 && h->d > d)
    return;
  h->f = f;
  h->d = d;
  h->v = v;
  h->a1 = hash_numbers.a1;
  h->yx = yx;
} 

/* void 
f_hash_put (char d, char f, short int v, short int yx, hash_num hash_numbers)
{
  register t_hentry *t,*h = &(v_hentry[m_hash_adr (hash_numbers.a0)]);
	int i, mindepth;

		i = 1;
		mindepth = 64;
		t = h;
	  while (h->a1 != hash_numbers.a1 && i < REHASH) {
			if (h->d < mindepth) {
				t = h;
				mindepth = h->d;
			}
			h++;
			i++;
		}
		
		if (h->a1 == hash_numbers.a1) {
			if (h->d > d)
				return;
			else {
				h->f = f;
				h->d = d;
				h->v = v;
				h->a1 = hash_numbers.a1;
				h->yx = yx;
			}
		}
		else {
			t->f = f;
			t->d = d;
			t->v = v;
			t->a1 = hash_numbers.a1;
			t->yx = yx;
		}
} */


/*
 *
 * get from hash
 *
 */

#ifdef __GNUC__
inline
#endif
/* short int 
f_hash_get (char d, char * f, short int * v, hash_num hash_numbers)
{
  register t_hentry *h = &(v_hentry[m_hash_adr (hash_numbers.a0)]);
	int i;

	i = 1;
  HashCall++;
	while (h->a1 != hash_numbers.a1 && i < REHASH) {
		h++;
		i++;
	}
  if (h->a1 != hash_numbers.a1)
    return 0;
	HashHit++;
  if (h->d < d) {
    *f = XX_HASH;
    return h->yx;
  }
  TransHit++;
  *f = h->f;
  *v = h->v;
  return h->yx;
} */

short int 
f_hash_get (char d, char * f, short int * v, hash_num hash_numbers)
{
  register t_hentry *h = &(v_hentry[m_hash_adr (hash_numbers.a0)]);

  HashCall++;
  if (h->a1 != hash_numbers.a1)
    return 0;
	HashHit++;
  if (h->d < d) {
    *f = XX_HASH;
    return h->yx;
  }
  TransHit++;
  *f = h->f;
  *v = h->v;
  return h->yx;
} 


/* void f_hash_board (int player, hash_num * hash_numbers, board * b)
{
  register int i,j,z;
  register long a0,a1;

  if (player == DARK) {
    a0 = H_B[0].a0p1;
    a1 = H_B[0].a1p1;
  } else {
    a0 = H_B[0].a0p2;
    a1 = H_B[0].a1p2;
  }
	for (i = 10; i <= 80; i += 10)
    for (j = 1; j <= 8; j++) {
			z = i + j;
			switch (b->sq[z]) {
			case DARK:
				a0 ^= H_B[z].a0p1;
				a1 ^= H_B[z].a1p1;
				break;
			case LIGHT:
				a0 ^= H_B[z].a0p2;
				a1 ^= H_B[z].a1p2;
				break;
			default:
				break;
    	}
		}

  hash_numbers->a0 = a0;
  hash_numbers->a1 = a1;
} */
void f_hash_board (int player, hash_num * hash_numbers, board * b)
{
  register int i,j;
  register long a0,a1;
	hash_square * has_poi = &H_B[11];
	char * square = &b->sq[11];

  if (player == DARK) {
    a0 = H_B[0].a0p1;
    a1 = H_B[0].a1p1;
  } else {
    a0 = H_B[0].a0p2;
    a1 = H_B[0].a1p2;
  }
	for (i = 1; i <= 8; i++) {
    for (j = 1; j <= 8; j++) {
			switch (*square++) {
			case DARK:
				a0 ^= has_poi->a0p1;
				a1 ^= has_poi->a1p1;
				break;
			case LIGHT:
				a0 ^= has_poi->a0p2;
				a1 ^= has_poi->a1p2;
				break;
			default:
				break;
    	}
			has_poi++;
		}
		has_poi += 2;
		square += 2;
	}

  hash_numbers->a0 = a0;
  hash_numbers->a1 = a1;
}



/******************************************************************/
/* dangerous bruges til at selektivt uddybe bestemte stillinger.  */
/* hvis det sidste treak var et hjoernetreak, og en af kanterne   */
/* op til dette hjoerne ar af typen "-*****-*" hvor - er et tomt  */
/* felt og * er en brik, da retuneres SAND ellers retuneres FALSK */
/******************************************************************/

short int dangerous(short int move, char pl, board *bd)
{
  short int i1,i2,i3,i4;

    i1 = bd->sq[11];
    i1 *= 3;
    i1 += bd->sq[12];
    i1 *= 3;
    i1 += bd->sq[13];
    i1 *= 3;
    i1 += bd->sq[14];
    i1 *= 3;
    i1 += bd->sq[15];
    i1 *= 3;
    i1 += bd->sq[16];
    i1 *= 3;
    i1 += bd->sq[17];
    i1 *= 3;
    i1 += bd->sq[18];
    sindex1 = i1;

    i2 = bd->sq[18];
    i2 *= 3;
    i2 += bd->sq[28];
    i2 *= 3;
    i2 += bd->sq[38];
    i2 *= 3;
    i2 += bd->sq[48];
    i2 *= 3;
    i2 += bd->sq[58];
    i2 *= 3;
    i2 += bd->sq[68];
    i2 *= 3;
    i2 += bd->sq[78];
    i2 *= 3;
    i2 += bd->sq[88];
    sindex2 = i2;

    i3 = bd->sq[81];
    i3 *= 3;
    i3 += bd->sq[82];
    i3 *= 3;
    i3 += bd->sq[83];
    i3 *= 3;
    i3 += bd->sq[84];
    i3 *= 3;
    i3 += bd->sq[85];
    i3 *= 3;
    i3 += bd->sq[86];
    i3 *= 3;
    i3 += bd->sq[87];
    i3 *= 3;
    i3 += bd->sq[88];
    sindex3 = i3;

    i4 = bd->sq[11];
    i4 *= 3;
    i4 += bd->sq[21];
    i4 *= 3;
    i4 += bd->sq[31];
    i4 *= 3;
    i4 += bd->sq[41];
    i4 *= 3;
    i4 += bd->sq[51];
    i4 *= 3;
    i4 += bd->sq[61];
    i4 *= 3;
    i4 += bd->sq[71];
    i4 *= 3;
    i4 += bd->sq[81];
    sindex4 = i4;

#ifdef NOEXTEN
  return FALSE;
  /* hvis NOEXTEN er defineret, da slaas uddybningen fra */
#endif

  if (pl == DARK) {
		switch (move)
			{
				case 11:
					return (hjo_trek[sindex1] & FARLIG_HJ1_SORT) | (hjo_trek[sindex4]  & FARLIG_HJ1_SORT);
				case 18:
					return (hjo_trek[sindex1] & FARLIG_HJ2_SORT) | (hjo_trek[sindex2]  & FARLIG_HJ1_SORT);
				case 81:
					return (hjo_trek[sindex4] & FARLIG_HJ2_SORT) | (hjo_trek[sindex3]  & FARLIG_HJ1_SORT);
				case 88:
					return (hjo_trek[sindex2] & FARLIG_HJ2_SORT) | (hjo_trek[sindex3]  & FARLIG_HJ2_SORT);
				default:
					return FALSE;
							/* det var ikke et hjoerne treak */
		}
	}
	else {
		switch (move)
			{
				case 11:
					return (hjo_trek[sindex1] & FARLIG_HJ1_HVID) | (hjo_trek[sindex4]  & FARLIG_HJ1_HVID);
				case 18:
					return (hjo_trek[sindex1] & FARLIG_HJ2_HVID) | (hjo_trek[sindex2]  & FARLIG_HJ1_HVID);
				case 81:
					return (hjo_trek[sindex4] & FARLIG_HJ2_HVID) | (hjo_trek[sindex3]  & FARLIG_HJ1_HVID);
				case 88:
					return (hjo_trek[sindex2] & FARLIG_HJ2_HVID) | (hjo_trek[sindex3]  & FARLIG_HJ2_HVID);
				default:
					return FALSE;
							/* det var ikke et hjoerne treak */
		}
	}
}

/**********************************************************************/
/* findmax2 er den normale alfa-beta algoritme, der bruges naar vi er */
/* ved en knude der ikke findes i det spiltrae vi gemmer, og vi ikke  */
/* har plads til at udvide vores spiltrae                             */
/**********************************************************************/

short int findmax2(short int player,
						short int look,
            short int depth,
            short int prevmov,
            movelist *list,
            board *bd,
            short int alfa,
            short int beta,
            short int *bestmove)
{
  movelist newlist;
  board newbd;
  short int i,maxscore,score,trek,junk,bound,hm,opp,olook = look;
	char bf = HI_HASH;
	hash_num h_n;

  aknud++;
	opp = other(player);

  if (list->nmoves > 0) {
    /* hvis der er lovlige traek */
		
		
#ifdef SELEXT
			if (look == D1 || look == D2) { 
					if (beta < 32000 && beta > -32000) {
				  	bound = beta + (abs(beta)*5)/10 + E_BOUND;
				  	score = findmax2(player,DP,depth, prevmov, list, bd,bound - 1, bound, bestmove);
						if (score >= bound) 
					  	return beta;
					}
						
					if (alfa < 32000 && alfa > -32000) {
						bound = alfa - (abs(alfa)*5)/10 - E_BOUND;
						score = findmax2(player,DP,depth, prevmov,list, bd,bound, bound + 1, bestmove);
						if (score <= bound)
							return alfa;
					}
			} 
#endif

		IF_HASH_GET (depth) {
      char hf;
      short int hv;
			
			f_hash_board (player, &h_n, bd);
      if ((hm = f_hash_get ((char)look, &hf, &hv, h_n)))
				if (f_legalmove (player,hm, bd)) {
					switch (hf) {
					case LO_HASH:
						if (hv >= alfa) 
							goto NEXT;
						break;
					case HI_HASH:
						if (hv < beta) 
							goto NEXT;
						break;
					case XX_HASH: 
						goto NEXT;
					}
					*bestmove = hm;
					return hv;
				} 
				else
					hm = 0;
		}
			else {
			  IF_HASH_PUT (depth)
				  f_hash_board (player, &h_n, bd);
				hm = 0;
			}

		NEXT:;


#ifndef NOEXTEN
    if ((list->nmoves == 1) && (look == 0))
      look++;
    /* hvis vi er ved en slutknude, og der kun er et mulig treak, saa */
    /* foroeger vi soegedybden med en, for at undersoege, om der er   */
    /* ved at opstaa en farlig situation                              */
#endif
    
    maxscore = -32767;
    sortlist(hm,prevmov, player,list,bd);
    /* sorter traekkene */
		
	/*	if (look == 0) {
			danmov = 0;
			i = 0;
			do {
				trek = list->move[i++];
				if (dangerous(trek, player, bd))
					newlist.move[danmov++] = trek;
			}
			while ((char)i < list->nmoves);
			if (danmov != 0) {
				for (i = 0; i < danmov; i++)
					list->move[i] = newlist.move[i];
				list->nmoves = danmov;
			}
		} */
		
		i = 0;
    do {
      trek = list->move[i++];
      /* naeste mulige treak */
      newbd = *bd;
      trymove(trek, player,&newbd);
        /* udfoer dette treak */
      if ((look == 0) && !dangerous(trek, (char)player,&newbd)) {
        /* hvis vi er ved en slutstilling, og denne ikke er "urolig" */
        /* saa evaluer denne                                         */
        score = -eval(-beta,-alfa,list->nmoves, opp,&newbd);
      }
      else {
       /* forsaet med naeste niveau i vores spiltrae */
        makelist(&newlist,opp,&newbd);
        /* kald findmax2 for at minimere */
				score = -findmax2(opp,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk);
        /* vi opdaterer responskiller vaerdierne paa foelgende maade */
        /* hvis score > alfa, saa var der ikke "cutoff" i niveauet   */
        /* lavere. Dette vil sige, at der sandsyneligvis ikke var    */
        /* noget saerligt godt treak, saa vi opdaterer med en lille  */
        /* vaerdi. Omvendt hvis der var "cutoff", saa var der et     */
        /* treak der hurtigt soergede for at standse soegningen, og  */
        /* der opdateres med en hoej vaerdi for dette treak          */
        if (player == computer) {
					if (score > alfa) {
							if (junk != 0)
							humres[trek-11][junk-11]++;
					}
					else
						if (junk != 0)
							humres[trek-11][junk-11] += 4;
				}
				else {
				  if (score > alfa) {
						if (junk != 0)
							comres[trek-11][junk-11]++;
					}
					else
						if (junk != 0)
							comres[trek-11][junk-11] += 4;
			 	}
      }
      if (score > maxscore) {
        /* nyt bedste treak, retuner dette, og opdater scoren */
					if (score >= beta)
						bf = HI_HASH;
					else if (score > alfa)
						bf = OK_HASH;
					else
						bf = LO_HASH; 
				if (score > alfa)
					alfa = score;
        maxscore = score;
        *bestmove = trek;
      }
					
      if (depth == 0) {
        /* hvis vi er ved dybde 0, da opdateres nogle globale */
        /* variable, sadan vi kan foelge hvordan der soeges   */
        /* uden at skulle vente paa at computeren er faerdig  */
        /* med at taenke                                      */
        sofar++;
        value = maxscore;
        if (timeout()) /* hvis tiden er gaaet saa stop */
          break;
      }
			if (tid_udlobet)
        /* her springer vi ud fra soegningen, hvis tiden er overskredet */
        /* for meget.                                                   */
        longjmp(env, 1);
    }
    while (((char)i < list->nmoves) && (maxscore < beta));
    /* indtil ikke flere treak eller cutoff */
			IF_HASH_PUT (depth) 
				f_hash_put ((char)olook, bf, maxscore, *bestmove, h_n);

  }
  else { /* short intet lovligt treak */
    *bestmove = 0;
    if (makelist(&newlist, opp, bd) == 0) {
      /* modstanderen har heller ingen lovlige treak, saa */
      /* spillet er slut                                  */
      aeval++;
      maxscore = (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
      if (maxscore < 0)
        maxscore -= 32600;
      else if (maxscore > 0)
        maxscore += 32600;
    }
    else
      return -findmax2(opp,look,depth,0,&newlist,bd,-beta,-alfa,&junk);
      /* vi soeger dybere */
  }
  return maxscore;
}


/**********************************************************************/
/* findmax1 er den normale alfa-beta algoritme, der bruges naar vi er */
/* ved en knude der ikke findes i det spiltrae vi gemmer, og vi har   */
/* plads til at udvide vores spiltrae                                 */
/**********************************************************************/

short int findmax1(short int player,
            short int look,
            short int depth,
            short int prevmov,
            movelist *list,
            board *bd,
            short int alfa,
            short int beta,
            short int *bestmove,
            tree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int maxscore,score,trek,junk,i, bound,hm,opp, olook = look;
	char bf = HI_HASH;
	hash_num h_n;
  tree *temp;

  aknud++;
	opp = other(player);

  if (list->nmoves > 0) {
    /* hvis der er lovlige traek */
	
#ifdef SELEXT	
			if (look == D1 || look == D2) { 
					if (beta < 32000 && beta > -32000) {
				  	bound = beta + (abs(beta)*5)/10 + E_BOUND;
				  	score = findmax2(player,DP,depth, prevmov, list, bd,bound - 1, bound, bestmove);
						if (score >= bound) 
					  	return beta;
					}
						
					if (alfa < 32000 && alfa > -32000) {
						bound = alfa - (abs(alfa)*5)/10 - E_BOUND;
						score = findmax2(player,DP,depth, prevmov,list, bd,bound, bound + 1, bestmove);
						if (score <= bound)
							return alfa;
					}
			} 
#endif

		IF_HASH_GET (depth) {
      char hf;
      short int hv;
			
			f_hash_board (player, &h_n, bd);
      if ((hm = f_hash_get ((char)look, &hf, &hv, h_n)))
				if (f_legalmove (player,hm, bd)) {
					switch (hf) {
					case LO_HASH:
						if (hv >= alfa) 
							goto NEXT;
						break;
					case HI_HASH:
						if (hv < beta) 
							goto NEXT;
						break;
					case XX_HASH: 
						goto NEXT;
					}
					*bestmove = hm;
					return hv;
				} 
				else
					hm = 0;
		}
			else {
			  IF_HASH_PUT (depth)
				  f_hash_board (player, &h_n, bd);
				hm = 0;
			}

		NEXT:;


#ifndef NOEXTEN
    if ((list->nmoves == 1) && (look == 0))
      look++;
    /* hvis vi er ved en slutknude, og der kun er et mulig treak, saa */
    /* foroeger vi soegedybden med en, for at undersoege, om der er   */
    /* ved at opstaa en farlig situation                              */
#endif
    maxscore = -32767;
    /* sorter traekkene */
    sortlist(hm,prevmov, player,list,bd);


    list_in_tree(list,spiltree);
    /* vi tilfoejer listen af mulige treak ved dette niveau, til vores */
    /* spiltrae                                                        */
/*		danmov = FALSE;

		if (look == 0) {
			i = 0;
			do {
				trek = list->move[i];
				if (dangerous(trek, player, bd)) {
					danmov = TRUE;
					farlige[i++] = TRUE;
				}
				else
					farlige[i++] = FALSE;
			}
			while ((char)i < list->nmoves);
		} */


    temp = (*spiltree);
		i = 0;
    do {
		/*	if (danmov) {
				while (!farlige[i++] && temp != NULL) 
					temp = temp->sosk;
					if (temp == NULL)
						break;
				} */
	
      trek = temp->move;
      /* naeste mulige treak */

      newbd = *bd;
      trymove(trek,player,&newbd);
      /* udfoer dette treak */

      if ((look == 0) && !dangerous(trek,(char)player,&newbd)) {
        /* hvis vi er ved en slutstilling, og denne ikke er "urolig" */
        /* saa evaluer denne                                         */
        score = -eval(-beta,-alfa,list->nmoves, opp,&newbd);
      }
      else {
        makelist(&newlist,opp,&newbd);
        /* hvis der stadig er plads til at udvide vores gemte spiltrae */
        /* saa kaldes findmin1 ellers kaldes findmin2                  */
				if (treeon && depth < SAVE_DEPTH)
					score = -findmax1(opp,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk,&temp->barn);
				else
					score = -findmax2(opp,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk);

        /* vi opdaterer responskiller vaerdierne paa foelgende maade */
        /* hvis score > alfa, saa var der ikke "cutoff" i niveauet   */
        /* lavere. Dette vil sige, at der sandsyneligvis ikke var    */
        /* noget saerligt godt treak, saa vi opdaterer med en lille  */
        /* vaerdi. Omvendt hvis der var "cutoff", saa var der et     */
        /* treak der hurtigt soergede for at standse soegningen, og  */
        /* der opdateres med en hoej vaerdi for dette treak          */
        if (player == computer) {
					if (score > alfa) {
							if (junk != 0)
							humres[trek-11][junk-11]++;
					}
					else
						if (junk != 0)
							humres[trek-11][junk-11] += 4;
				}
				else {
				  if (score > alfa) {
						if (junk != 0)
							comres[trek-11][junk-11]++;
					}
					else
						if (junk != 0)
							comres[trek-11][junk-11] += 4;
			 	}
    	}
      if (score > maxscore) {
        /* nyt bedste treak, retuner dette, saet det forest i listen */
        /* og opdater scoren                                         */
					if (score >= beta)
						bf = HI_HASH;
					else if (score > alfa)
						bf = OK_HASH;
					else
						bf = LO_HASH; 

        if (score > alfa)
          alfa = score;
        maxscore = score;
        *bestmove = trek;
        put_in_front(spiltree, &temp);
      }
      temp = temp->sosk;
		/*	i++; */
      /* naeste treak */
      if (depth == 0) {
        sofar++;
        value = maxscore;
        if (timeout()) /* er tiden gaaet */
          break;
      }
      if (tid_udlobet)
        /* her springer vi ud fra soegningen, hvis tiden er overskredet */
        /* for meget.                                                   */
        longjmp(env, 1);
    }
    while ((temp != NULL) && (maxscore < beta));
    /* indtil ikke flere treak eller cutoff */
			IF_HASH_PUT (depth) 
				f_hash_put ((char)olook, bf, maxscore, *bestmove, h_n);

  }
  else { /* short intet lovligt treak */
    *bestmove = 0;
    make_node(spiltree);
    /* tilfoej en "dummy" knude til vores trea */
    if (makelist(&newlist, opp, bd) == 0) {
      /* modstanderen har heller ingen lovlige treak, saa */
      /* spillet er slut                                  */
      aeval++;
      maxscore = (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
      if (maxscore < 0)
        maxscore -= 32600;
      else if (maxscore > 0)
        maxscore += 32600;
    }
    else {
      /* hvis der stadig er plads til at udvide vores gemte spiltrae */
      /* saa kaldes findmin1 ellers kaldes findmin2                  */
      if (treeon && depth < SAVE_DEPTH)
        return -findmax1(opp,look,depth,0,&newlist,bd,-beta,-alfa,&junk,&(*spiltree)->barn);
      else
        return -findmax2(opp,look,depth,0,&newlist,bd,-beta,-alfa,&junk);
    }
  }
  return maxscore;
}

/**********************************************************************/
/* findmax1 er den normale alfa-beta algoritme, der bruges naar vi er */
/* ved en knude der findes i det spiltrae vi gemmer                   */
/**********************************************************************/

short int findmax(short int player,
						short int look,
            short int depth,
            board *bd,
            short int alfa,
            short int beta,
            short int *bestmove,
            tree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int maxscore,score,trek,junk,nm, bound, bf = HI_HASH,hm,opp, olook = look;
	hash_num h_n;
  tree *temp;

  aknud++;
  temp = (*spiltree);
	opp = other(player);


  if (temp->move != 0) {
    /* hvis der er lovlige traek */
		
		
#ifdef SELEXT
				if (look == D1 || look == D2) { 
					makelist(&newlist,player, bd);
					if (beta < 32000 && beta > -32000) {
				  	bound = beta + (abs(beta)*5)/10 + E_BOUND;
				  	score = findmax2(player,DP,depth, 0, &newlist, bd,bound - 1, bound, bestmove);
						if (score >= bound) 
					  	return beta;
					}
						
					if (alfa < 32000 && alfa > -32000) {
						bound = alfa - (abs(alfa)*5)/10 - E_BOUND;
						score = findmax2(player,DP,depth, 0,&newlist, bd,bound, bound+1, bestmove);
						if (score <= bound)
							return alfa;
					}
			} 
			
#endif

		IF_HASH_GET (depth) {
      char hf;
      short int hv;
			
			f_hash_board (player, &h_n, bd);
      if ((hm = f_hash_get ((char)look, &hf, &hv, h_n)))
				if (f_legalmove (player,hm, bd)) {
					switch (hf) {
					case LO_HASH:
						if (hv >= alfa) 
							goto NEXT;
						break;
					case HI_HASH:
						if (hv < beta) 
							goto NEXT;
						break;
					case XX_HASH: 
						goto NEXT;
					}
					if (temp->move != hm) {
						do
							temp = temp->sosk;
						while (temp != NULL && temp->move != hm);
						if (temp != NULL)
							put_in_front(spiltree, &temp);
					}
					*bestmove = hm;
					return hv;
				} 
				else
					hm = 0;
		}
			else {
			  IF_HASH_PUT (depth)
				  f_hash_board (player, &h_n, bd);
				hm = 0;
			}

		NEXT:;



    if (look == 0) {
      /* hvis vi er ved en slutknude, saa skal vi taelle de mulige treak */
      /* , da dette antal skal bruges ved evalueringen                   */
      nm = 1;
      while (temp->sosk != NULL) {
        temp = temp->sosk;
        nm++;
      }
      temp = (*spiltree);
#ifndef NOEXTEN
    /* hvis vi er ved en slutknude, og der kun er et mulig treak, saa */
    /* foroeger vi soegedybden med en, for at undersoege, om der er   */
    /* ved at opstaa en farlig situation                              */
      if (nm == 1)
        look++;
#endif
    }
    maxscore = -32767;
    do {
      trek = temp->move;
      /* naeste mulige treak */

      newbd = *bd;
      trymove(trek,player,&newbd);
			if (depth == 0 && look > 6)
				make_try(trek,varlook + 1);
        /* udfoer dette treak */

      if ((look == 0) && !dangerous(trek,(char)player,&newbd)) {
        /* hvis vi er ved en slutstilling, og denne ikke er "urolig" */
        /* saa evaluer denne                                         */
        score = -eval(-beta,-alfa,nm,opp,&newbd);
      }
      else  {
        /* hvis vi er ved en knude, hvis barn findes i det gemte trae */
        /* saa kaldes findmin                                         */
        if (temp->barn != NULL) 
					score = -findmax(opp,look > 0 ? look - 1 : 0,depth + 1,&newbd,-beta,-alfa,&junk,&temp->barn);		
        else {
          makelist(&newlist,opp,&newbd);
          /* hvis barnet ikke findes i vores trae, undersoeges om der er   */
          /* mere plads, og hvis der er det kaldes findmin1, ellers kaldes */
          /* findmin2                                                      */
       		if (treeon && depth < SAVE_DEPTH)
          	score = -findmax1(opp,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk,&temp->barn);
        	else
          	score = -findmax2(opp,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk);
				}
        /* vi opdaterer responskiller vaerdierne paa foelgende maade */
        /* hvis score > alfa, saa var der ikke "cutoff" i niveauet   */
        /* lavere. Dette vil sige, at der sandsyneligvis ikke var    */
        /* noget saerligt godt treak, saa vi opdaterer med en lille  */
        /* vaerdi. Omvendt hvis der var "cutoff", saa var der et     */
        /* treak der hurtigt soergede for at standse soegningen, og  */
        /* der opdateres med en hoej vaerdi for dette treak          */
				if (player == computer) {
					if (score > alfa) {
							if (junk != 0)
							humres[trek-11][junk-11]++;
					}
					else
						if (junk != 0)
							humres[trek-11][junk-11] += 4;
				}
				else {
				  if (score > alfa) {
						if (junk != 0)
							comres[trek-11][junk-11]++;
					}
					else
						if (junk != 0)
							comres[trek-11][junk-11] += 4;
			 }

      }
      if (score > maxscore) {
        /* nyt bedste treak, retuner dette, saet det forest i listen */
        /* og opdater scoren                                         */
        maxscore = score;
        *bestmove = trek;
					if (score >= beta)
						bf = HI_HASH;
					else if (score > alfa)
						bf = OK_HASH;
					else
						bf = LO_HASH;  
						
				if (score > alfa)
          alfa = score;
        put_in_front(spiltree, &temp);
      }
      if (depth == 0) {
        sofar++;
        value = maxscore;
				if (look > 6)
					make_res(maxscore, 0); 
        if (timeout()) /* er tiden gaaet */
          break;
      }
      temp = temp->sosk;
      /* naeste treak */

      if (tid_udlobet)
        /* her springer vi ud fra soegningen, hvis tiden er overskredet */
        /* for meget.                                                   */
        longjmp(env, 1);
    }
    while ((temp != NULL) && (maxscore < beta));
    /* indtil ikke flere treak eller cutoff */
		
			IF_HASH_PUT (depth) 
				f_hash_put ((char)olook, (char)bf, maxscore, *bestmove, h_n);

  }
  else { /* short intet lovligt treak */
    *bestmove = 0;
    if (temp->barn != NULL)
      /* hvis der er et barn, kaldes findmin */
      return -findmax(opp,look,depth,bd,-beta,-alfa,&junk,&temp->barn);
    else {
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
        /* hvis barnet ikke findes i vores trae, undersoeges om der er   */
        /* mere plads, og hvis der er det kaldes findmin1, ellers kaldes */
        /* findmin2                                                      */
        if (treeon && depth < SAVE_DEPTH)
          return -findmax1(opp,look,depth,0,&newlist,bd,-beta,-alfa,&junk,&temp->barn);
        else
          return -findmax2(opp,look,depth,0,&newlist,bd,-beta,-alfa,&junk);
      }
    }
  }
  return maxscore;
}


short int zero_findmax(short int player,
            short int look,
            short int depth,
            board *bd,
            short int alfa,
            short int beta,
            short int *bestmove,
            tree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int maxscore,score,trek,junk,nm,oldbeta,research,ores;
  tree *temp,*rtemp;


  aknud++;
  temp = (*spiltree);


  if (look == 0) {
    nm = 1;
    while (temp->sosk != NULL) {
      temp = temp->sosk;
      nm++;
    }
    temp = (*spiltree);
#ifndef NOEXTEN
    if (nm == 1)
      look++;
#endif
  }
  oldbeta = beta;
  ores = FALSE;
  tryagain:

  trek = temp->move;
  newbd = *bd;
  trymove(trek,computer,&newbd);
	if (look > 6)
		make_try(trek,varlook + 1);

  if ((look == 0) && !dangerous(trek,(char)computer,&newbd)) {
    score = -eval(-beta,-alfa,nm,human,&newbd);
    if (score > alfa)
      alfa = score;
  }
  else  {
    if (temp->barn != NULL)
      score = -findmax(human,look > 0 ? look - 1 : 0,depth + 1,&newbd,-beta,-alfa,&junk,&temp->barn);
    else {
      makelist(&newlist,human,&newbd);
      if (treeon && depth < SAVE_DEPTH)
        score = -findmax1(human,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk,&temp->barn);
      else
        score = -findmax2(human,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk);
    }
    if (score > alfa) {
      alfa = score;
      if (junk != 0)
        humres[trek-11][junk-11]++;
    }
    else
      if (junk != 0)
        humres[trek-11][junk-11] += 4;
  }

  maxscore = score;
  *bestmove = trek;

  sofar++;
  value = maxscore;
	search_time = clock();

	search_time -= gam_tid;
	make_res(maxscore, search_time);

  if (timeout())
    return maxscore;
  temp = temp->sosk;
  if ((temp == NULL) || (maxscore >= beta))
    return maxscore;

  beta = alfa + 1;
  research = 0;
  do {
    trek = temp->move;
    newbd = *bd;
    trymove(trek,computer,&newbd);
		if (look > 6)
			make_try(trek,varlook + 1);

    if ((look == 0) && !dangerous(trek,(char)computer,&newbd)) {
      score = -eval(-beta,-alfa,nm,human,&newbd);
      if (score > alfa) {
        alfa = score;
        beta = alfa + 1;
        research++;
        if (research > 1) {
          rtemp = temp->sosk;
          put_at_p2(spiltree, &temp);
          temp = (*spiltree);
          beta = oldbeta;
          sofar -= 1;
          if (alfa >= oldbeta) {
            *bestmove = trek;
            return alfa;
          }
          ores = TRUE;
          goto tryagain;
        }
      }
    }
    else  {
      if (temp->barn != NULL)
        score = -findmax(human,look > 0 ? look - 1 : 0,depth + 1,&newbd,-beta,-alfa,&junk,&temp->barn);
      else {
        makelist(&newlist,human,&newbd);
        if (treeon && depth < SAVE_DEPTH)
          score = -findmax1(human,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk,&temp->barn);
        else
          score = -findmax2(human,look > 0 ? look - 1 : 0,depth + 1,RESPON,&newlist,&newbd,-beta,-alfa,&junk);
      }
      if (score > alfa) {
        alfa = score;
        beta = alfa + 1;
        research++;
        if (junk != 0)
          humres[trek-11][junk-11]++;
        if (research > 1) {
          rtemp = temp->sosk;
          put_at_p2(spiltree, &temp);
          temp = (*spiltree);
          beta = oldbeta;
          sofar -= 1;
          if (alfa >= oldbeta){
            *bestmove = trek;
            return alfa;
          }
          ores = TRUE;
					if (look > 6) {
            search_time = clock();
						search_time -= gam_tid;
						make_res(maxscore, search_time);
					}

          goto tryagain;
        }
      }
      else
        if (junk != 0)
          humres[trek-11][junk-11] += 4;
    }
    if (score > maxscore) {
      maxscore = score;
      *bestmove = trek;
      put_in_front(spiltree, &temp);
		}

		if (look > 6) {
      search_time = clock();
      search_time -= gam_tid;
			make_res(maxscore, search_time);
		}

    sofar++;
    value = maxscore;
    if (timeout())
      break;
    if (ores) {
      ores = FALSE;
      temp = rtemp;
    }
    else
      temp = temp->sosk;
  }
  while ((temp != NULL) && (maxscore < oldbeta));

  return maxscore;
}


/******************************************************************/
/* Her er den optimerede version af slutspilsalgoritmen. Der er   */
/* brugt all mulige tricks for at undgaa undnoedvendigt arbejde   */
/* dette goer desvaerre ogsaa koden ret ulaeselig, men ogsaa      */
/* meget hurtigere.                                               */
/******************************************************************/

/* utrylegal tages i brug, naar der kun er 2 tomme felter paa braeddet */
/* foerst udfores treeakket "trysq af spilleren "pl", (vi ved, at det  */
/* er et lovligt treak) dernaest undersoeges om modstanderen kan fore- */
/* tage et treak paa det sidste tomme felt. Hvis han kan dette saa     */
/* proeves dette treak, og stillingen evalueres. Det skal bemaerkes,   */
/* at det sidste treak ikke udfoeres paa stillingen, man taeller blot  */
/* hvor mange brikker der vendes, da vi ikke skal bruge den resulte-   */
/* rende stilling til noget. Hvis det tomme felt ikke er et lovligt    */
/* treak for modstanderen, saa undersoeges det om det er et lovligt    */
/* for spilleren, og i saa tilfaelde evalueres stillingen. hvis ikke,  */
/* saa evalueres stillingen paa sidste stilling, da ingen af spillerne */
/* kunne foretage noget treak. Selve aendringerne retuneres i "moves"  */
/* til genetabelering af de gamle stilling.                            */
/* Jeg skal til denne og den foelgende routine bemaerke, at selve      */
/* programmerings teknikken absolut ikke er anbefalingsvaerdig, og kun */
/* er lavet som et eksperiment i optimering. Koden er fuldstaendig     */
/* ulaeselig, og jeg er ikke sikker paa, at den virker paa alle c-     */
/* oversaettere. Det eneste jeg kan sige, er at den virker pa gnu c    */
/* oversaetteren og at den er hurtigere end den fornuftige kode.       */


char utrylegal(short int pl, board *bd, short int moves[])
{
  short int trysq,k1,opp,del,*delpo;
  char diff,*sqpoi;

  trysq = bd->possible.move[0];
  opp = other(pl);

  delpo = dirs[trysq];
  /* delpo peger nu paa de retninger hvor det er teoretisk muligt */
  /* at vende brikker, plus de retninger, hvor der kan vaere et   */
  /* nyt tomt felt, vi kan her bruge "dirs" arrayet, da vi ved at */
  /* der ingen nye felter er at tilfoeje listen af mulige treak   */
  /* da der kun er 2 tomme felter                                 */

  del = *delpo++;
  /* del er den foerste af disse retninger */
  do {
    k1 = trysq + del;
    /* k1 er nu index til det foerste felt i denne retning */

    if (bd->sq[k1] == (char)opp) {
      /* hvis dette felt har en modstander brik */

      do k1 += del;
      while (bd->sq[k1] == (char)opp);
      /* spring resten af modstanderbrikkerne over */

      if (bd->sq[k1] == (char)pl) {
        /* hvis naeste felt har en spiller brik, saa er treakket */
        /* lovligt                                               */

        *moves++ = trysq;
        /* vi gaemmer det felt som vi saetter en brik paa, det skal senere */
        /* saettes til et tomt felt, hvis vi skal bruge stillingen igen    */

        diff = bd->ndiscs[pl] - 31;
        diff += diff;
        /* diff er nu forskellen i brikker, beregnet ved */
        /* diff = 2*(discs(pl) - 31)                     */
        goto legal;
      }
    }
  } while ((del = *delpo++) != 0);
  /* indtil ikke flere retninger */

  return -100;

  do {
    k1 = trysq + del;
    /* k1 er nu index til det foerste felt i denne retning */

    if (bd->sq[k1] == (char)opp) {
      /* hvis dette felt har en modstander brik */

      do k1 += del;
      while (bd->sq[k1] == (char)opp);
      /* spring resten af modstanderbrikkerne over */

      if (bd->sq[k1] == (char)pl) {
        /* hvis naeste felt har en spiller brik, saa er treakket */
        /* lovligt, saa vi vender modstander brikkerne og holder */
        /* rede paa hvor mange der er vendt, og hvilke der er    */
        /* vendt, da vi begegner forskellen i brikker skal vi    */
        /* laegge 2 til hvergang vi vender en brik               */

legal:  k1 -= del;
        /* vi skal lige et skridt tilbage, da vi peger paa den */
        /* spiller brik der bruges til at vende brikkerne med  */
        do {
          bd->sq[k1] = (char)pl;
          *moves++ = k1;
          diff += 2;
          k1 -= del;
        }
        while (k1 != trysq);
        /* vi vender brikker indtil vi er tilbage ved vores */
        /* udgangs felt                                     */
      }
    }
  } while ((del = *delpo++) != 0);
  /* indtil ikke flere retninger */

  *moves = 0;
  /* vi saetter den sidste ikke brugte plads i "moves" til 0 for */
  /* at markere slutningen paa de vendte brikker                 */


  aeval++;
  bd->sq[trysq] = (char)pl;
  /* vi saetter brikken paa braeddet. Egentlig skulle vi her laegge    */
  /* 1 til diff, da spilleren "pl" lige har faaet en brik paa braeddet */
  /* Dette goeres i stedet et andet sted, da de kan spare en addition  */

  trysq = bd->possible.move[1];


  sqpoi = (char *)(&bd->sq[trysq]);
  /* sqpoi peger nu paa dettte felt */

  delpo = dirs[trysq];
  /* delpo peger nu paa de retninger hvor det teoretisk */
  /* er muligt at vende brikker                         */

  del = *delpo++;
  /* del er nu den foerste retning  */

  /* vi bruger nu et saerdeles grimt trick. Vi undersoeger om treakket */
  /* er lovligt paa den saedvanlige maade, ved at for hver retning at  */
  /* undersoege om der kan vendes brikker. Hvis vi kommer til en ret-  */
  /* ning hvor dette kan goeres, saa springer vi direkte in til midten */
  /* af en anden loekke der saa taeller hvor mange brikker der kan     */
  /* vendes.                                                           */
  do {
    if (sqpoi[del] == (char)pl) {
      trysq = del;
      do trysq += del;
      while (sqpoi[trysq] == (char)pl);
      if (sqpoi[trysq] == (char)opp) {
      /* treakket var lovligt, saa modstanderen kunne saette en brik her */
      /* Denne brik ophaever den brik der blev sat af spilleren ifoer,   */
      /* saa nu stemmer vores variabel diff.                             */
        goto ok;
      }
    }
  } while ((del = *delpo++) != 0);

  /* hvis vi naar her, saa kunne modstanderen ikke foretage et treak paa  */
  /* dette felt, og vi undersoeger, om spilleren kan fortage et treak her */

  delpo -= 2;
  /* vi benytter her, at delpo peger 2 pladser efter den sidste retning */
  /* vi flytter saa blot 2 pladser tilbage, og gaar baglaens i listen.  */
  /* elementet foer det foerste element antages at vaere 0. Det gaelder */
  /* med den nuvaerende compiler arkitektur, at elementerne i en 2-     */
  /* dimensionel array ligger fortlobende i hukkomelsen, og ifoelge     */
  /* ANSI c skal et array initialeseres til 0, derfor skulle det ogsaa  */
  /* gaelde. Denne programmeringsteknik kan dog ikke anbefales :-)      */

  del = *delpo;
  do {
    if (sqpoi[del] == (char)opp) {
      trysq = del;
      do trysq += del;
      while (sqpoi[trysq] == (char)opp);
      if (sqpoi[trysq] == (char)pl) {
        diff += 2;
        /* treakket var lovligt, saa spilleren kunne saette en brik her   */
        /* spilleren har nu sat 2 brikker som vi ikke har talt med, dette */
        /* gores nu, hvorefter vi springer ind midt i en anden routine    */
        /* der taeller hvor mange brikker spilleren ellers kunne vende    */
        goto ok1;
      }
    }
  } while ((del = *(--delpo)) != 0);

  aknud++;
  return ++diff;

  /* hvis vi naar her, saa var det eneste lovlige treak det foerste.  */
  /* Vi skal opdatere diff med 1, da det ikke er blevet gjordt, og    */
  /* aknud opdateres med en, da vi kun har naaet til en ny knude      */

  /* her taelles hvor mange brikker spilleren kunne vende ved sit */
  /* andet treak                                                  */
  do {
    if (sqpoi[del] == (char)opp) {
      trysq = del;
      do trysq += del;
      while (sqpoi[trysq] == (char)opp);
      if (sqpoi[trysq] == (char)pl) {
ok1:    trysq -= del;
        do
          diff += 2;
        while ((trysq -= del) != 0);
      }
    }
  } while ((del = *(--delpo)) != 0);
  aknud += 3;
  /* aknud opdateres med 3, forst den knude med spillerens foerste treak */
  /* saa en dummy knude hvor modstanderen ikke kunne treakke, og saa     */
  /* spillerens andet treak. (Bruges til at teste hvor mange knuder der  */
  /* besoeges og genereres                                               */

  return diff;

  /* her taelles hvor mange brikker modstanderen kunne vende ved sit treak */
  do {
    if (sqpoi[del] == (char)pl) {
      trysq = del;
      do trysq += del;
      while (sqpoi[trysq] == (char)pl);
      if (sqpoi[trysq] == (char)opp) {
ok:     trysq -= del;
        do
          diff -= 2;
        while ((trysq -= del) != 0);
      }
    }
  } while ((del = *delpo++) != 0);
  aknud += 2;

  /* aknud opdateres med 2, forst knuden med spillerens treak */
  /* og saa modstanderen treak.                               */

  return diff;
}



/*******************************************************************/
/* ftrylegal er noejagtigt som utrylegal, den eneste forskel er at */
/* aendringerne i stillingen ikke retuneres. Dette skyldes, at     */
/* ftrylegal kun kaldes, naar stillingen ikke skal bruges igen     */
/*******************************************************************/

char ftrylegal(short int pl, board *bd)
{
  short int trysq,k1,opp,del,*delpo;
  char diff,*sqpoi;

  trysq = bd->possible.move[1];
  opp = other(pl);

  delpo = dirs[trysq];
  del = *delpo++;


  do {
    k1 = trysq + del;
    if (bd->sq[k1] == (char)opp) {
      do k1 += del;
      while (bd->sq[k1] == (char)opp);
      if (bd->sq[k1] == (char)pl) {
        diff = bd->ndiscs[pl] - 31;
        diff += diff;
        goto legal;
      }
    }
  } while ((del = *delpo++) != 0);

  return -100;

  do {
    k1 = trysq + del;
    if (bd->sq[k1] == (char)opp) {
      do k1 += del;
      while (bd->sq[k1] == (char)opp);
      if (bd->sq[k1] == (char)pl) {
legal:  k1 -= del;
        do {
          bd->sq[k1] = (char)pl;
          diff += 2;
          k1 -= del;
        }
        while (k1 != trysq);
      }
    }
  } while ((del = *delpo++) != 0);

  aeval++;
  bd->sq[trysq] = (char)pl;

  trysq = bd->possible.move[0];
  sqpoi = (char *)(&bd->sq[trysq]);
  delpo = dirs[trysq];
  del = *delpo++;
  do {
    if (sqpoi[del] == (char)pl) {
      trysq = del;
      do trysq += del;
      while (sqpoi[trysq] == (char)pl);
      if (sqpoi[trysq] == (char)opp) {
        goto ok;
      }
    }
  } while ((del = *delpo++) != 0);

  delpo -= 2;
  del = *delpo;
  do {
    if (sqpoi[del] == (char)opp) {
      trysq = del;
      do trysq += del;
      while (sqpoi[trysq] == (char)opp);
      if (sqpoi[trysq] == (char)pl) {
        diff += 2;
        goto ok1;
      }
    }
  } while ((del = *(--delpo)) != 0);

  aknud++;
  return ++diff;

  do {
    if (sqpoi[del] == (char)opp) {
      trysq = del;
      do trysq += del;
      while (sqpoi[trysq] == (char)opp);
      if (sqpoi[trysq] == (char)pl) {
ok1:    trysq -= del;
        do {
          diff += 2;
        } while ((trysq -= del) != 0);
      }
    }
  } while ((del = *(--delpo)) != 0);
  aknud += 3;
  return diff;

  do {
    if (sqpoi[del] == (char)pl) {
      trysq = del;
      do trysq += del;
      while (sqpoi[trysq] == (char)pl);
      if (sqpoi[trysq] == (char)opp) {
ok:     trysq -= del;
        do
          diff -= 2;
        while ((trysq -= del) != 0);
      }
    }
  } while ((del = *delpo++) != 0);
  aknud += 2;
  return diff;
}

#define TRESH 3

/***********************************************************************/
/* slutmax2 er en speical algoritme der bruges, naar der er mindre end */
/* 3 tomme felter paa braeddet. Naar vi ved dette kan vi antage nogle  */
/* ting, der goer det hurtigere at evaluere stillingen                 */
/***********************************************************************/
char slutmin2(board *bd,
             char alfa,
             char beta,
             short int *bestmove);

char slutmax2(board *bd,
             char alfa,
             char beta,
             short int *bestmove)
{
  short int junk,moves[30];
  char maxscore,score;

  aknud++;

  /* vi ved at der kun er 2 tomme felter. Vi kalder utrylegal      */
  /* der undeersoeger om det er muligt at foretage et treak, og    */
  /* hvis det er tilfaeldet saa evaluerers stillingen og desuden   */
  /* retunerer de aendringer der blev gjordt paa stillingen saa vi */
  /* kan komme tilbage til den gamle stiling igen, hvis det er     */
  /* noeadvendigt. Hvis treakket ikke var lovligt saa returneres   */
  /* vaerdien -100. i dette tilfaelde blev der ikke aendret noget  */
  /* paa braeddet                                                  */

  maxscore = utrylegal(computer,bd,moves);

  *bestmove = bd->possible.move[0];
  if (maxscore >= beta)
    /* hvis der er cutoff, saa retunerer vi scoren */
    return maxscore;

  if (maxscore > -100) {
    /* ellers genetabeleres den gamle stilling */
    short int move,pl,* movie;
    pl = human;
    movie = moves;

    move = *movie++;
    bd->sq[move] = EMPTY;
    /* slet den brik der blev sat */
    move = *movie++;
    do
     bd->sq[move] = (char)pl;
    while ((move = *movie++) != 0);
    /* vend de vendte brikker tilbage */
  }

  score = ftrylegal(computer,bd);
  /* her proeaves det sidste treak, og stillingen evalueres */
  if (score > maxscore) {
    /* hvis nyt bedste treak, saa opdater dette og retuner scoren */
    *bestmove = bd->possible.move[1];
    return score;
  }

  if (maxscore > -100)
    return maxscore;

  /* ingen lovlige treak */
  *bestmove = 0;
  if (legalmoves(human, bd) == FALSE) {
    /* modstanderen har heller ingen lovlige treak, saa */
    /* spillet er slut                                  */
    aeval++;
    return (char)(bd->ndiscs[computer] - bd->ndiscs[human]);
  }
  else
    return slutmin2(bd,alfa,beta,&junk);
    /* vi kalder slutmin2 for at minimere */
}


/****************************************************************/
/* slutmin2 er som slutmax2 blot minimeres der her              */
/****************************************************************/

char slutmin2(board *bd,
             char alfa,
             char beta,
             short int *bestmove)
{
  short int junk,moves[30];
  char minscore,score;


  aknud++;


  minscore = -utrylegal(human,bd,moves);

  *bestmove = bd->possible.move[0];

  if (minscore <= alfa)
    return minscore;

  if (minscore < 100) {
    short int move,pl,* movie;
    pl = computer;
    movie = moves;

    move = *movie++;
    bd->sq[move] = EMPTY;
    move = *movie++;
    do
     bd->sq[move] = (char)pl;
    while ((move = *movie++) != 0);
  }

  score = -ftrylegal(human,bd);

  if (score < minscore) {
    *bestmove = bd->possible.move[1];
    return score;
  }

  if (minscore < 100)
    return minscore;

  *bestmove = 0;
  if (legalmoves(computer, bd) == FALSE) {
    aeval++;
    return (char)(bd->ndiscs[computer] - bd->ndiscs[human]);
  }
  else
    return slutmax2(bd,alfa,beta,&junk);
}



/***********************************************************************/
/* slutmax3 er en speical algoritme der bruges, naar der er mindre end */
/* 3 tomme felter paa braeddet. Naar vi ved dette kan vi antage nogle  */
/* ting, der goer det hurtigere at evaluere stillingen                 */
/***********************************************************************/
char slutmax3(short int player,
						 board *bd,
             char alfa,
             char beta)
{
  short int moves[30],opp;
  char maxscore,score;

  aknud++;

  /* vi ved at der kun er 2 tomme felter. Vi kalder utrylegal      */
  /* der undeersoeger om det er muligt at foretage et treak, og    */
  /* hvis det er tilfaeldet saa evaluerers stillingen og desuden   */
  /* retunerer de aendringer der blev gjordt paa stillingen saa vi */
  /* kan komme tilbage til den gamle stiling igen, hvis det er     */
  /* noeadvendigt. Hvis treakket ikke var lovligt saa returneres   */
  /* vaerdien -100. i dette tilfaelde blev der ikke aendret noget  */
  /* paa braeddet                                                  */

  maxscore = utrylegal(player,bd,moves);

  if (maxscore >= beta)
    /* hvis der er cutoff, saa retunerer vi scoren */
    return maxscore;
	
	opp = other(player);

  if (maxscore > -100) {
    /* ellers genetabeleres den gamle stilling */
    short int move,* movie;
    
    movie = moves;

    move = *movie++;
    bd->sq[move] = EMPTY;
    /* slet den brik der blev sat */
    move = *movie++;
    do
     bd->sq[move] = (char)opp;
    while ((move = *movie++) != 0);
    /* vend de vendte brikker tilbage */
  }

  score = ftrylegal(player,bd);
  /* her proeaves det sidste treak, og stillingen evalueres */
  if (score > maxscore)
    /* hvis nyt bedste treak, saa opdater dette og retuner scoren */
    return score;

  if (maxscore > -100)
    return maxscore;

  /* ingen lovlige treak */
  if (legalmoves(opp, bd) == FALSE) {
    /* modstanderen har heller ingen lovlige treak, saa */
    /* spillet er slut                                  */
    aeval++;
    return (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
  }
  else
    return -slutmax3(opp,bd,-beta,-alfa);
    /* vi kalder slutmin2 for at minimere */
}

/*****************************************************************/
/* her er alfa-beta algoritmen for slutspillet, her skal kun     */
/* beregnes forskellen i brikker.                                */
/* slutmax1 er den optimerede variant der bruges, naar vi naar   */
/* til knuder der ikke findes i det spiltrae som gemmes.         */
/* Denne version bruges naar FASTSLUT er defineret, vi skal ikke */
/* holde rede med hvor dybt i vores soegetrae vi er, da vi her   */
/* blot soeger til ingen lovlige treak er, eller til der kun er  */
/* to tomme felter paa braeddet hvorefter en special routine     */
/* (slutmax3) bruges til at evaluere disse                       */
/*****************************************************************/

char slutmax1(short int player,
              short int look,
              short int depth,
             board *bd,
             char alfa,
             char beta,
             short int *bestmove,
             short int killer)
{
  board newbd;
  movelist list;
  short int i,trek,junk,newkil,hm,opp;
  char maxscore,score,bf = HI_HASH;
	hash_num h_n;

  aknud++;
	opp = other(player);
	
		IF_END_HASH_GET (depth) {
      char hf;
      short int hv;
			
			f_hash_board (player, &h_n, bd);
      if ((hm = f_hash_get ((char)look, &hf, &hv, h_n)))
				if (f_legalmove (player,hm, bd)) {
					switch (hf) {
					case LO_HASH:
						if (hv >= alfa) {
							killer <<= 8;
						  killer |= hm;
							goto NEXT;
						}
						break;
					case HI_HASH:
						if (hv < beta) {
							killer <<= 8;
						  killer |= hm;
							goto NEXT;
						}
						break;
					case XX_HASH: 
						killer <<= 8;
						killer |= hm;
						goto NEXT;
					}
					*bestmove = hm;
					return (char)hv;
				} 
				else
					hm = 0;
		}
			else
				hm = 0;

		NEXT:;


	makelist(&list, player, bd);

  if (list.nmoves > 0) {
    /* hvis der er lovlige traek */

		look--;

    if (list.nmoves == 1) {
      /* hvis der kun er 1 treak, saa behoeves vi ikke at lave en */
      /* kopi af stillingen, da vi ikke skal bruge den mere end   */
      /* en gang                                                  */

      /* da vi har en special algoritme, til at evaluere de sidste  */
      /* treak, ved vi at vi ikke er ved slutningen af spiltraet,   */
      /* saa vi behoeve ikke noget kode til at evaluere en stilling */
      /* her                                                        */

      /* hvis der er mindre end "TRESH" (3) tomme felter kalder   */
      /* vi slutmin2, der er en special algoritme til evaluering  */
      /* af stillinger med mindre en 3 tomme fleter, ellers       */
      /* kaldes slutmin1                                          */

      if (look <= 1) {
        ftrymove(*bestmove = list.move[0], player,bd);
        return -slutmax3(opp,bd,-beta,-alfa);
      }
      else {
        trymove(*bestmove = list.move[0], player,bd);
        maxscore = -slutmax1(opp,look,depth + 1,bd,-beta,-alfa,&junk,0);
				IF_END_HASH_PUT (depth) {
					if (maxscore >= beta)
						bf = HI_HASH;
					else if (maxscore > alfa)
						bf = OK_HASH;
					else
						bf = LO_HASH;
					f_hash_put (look + 1, bf, maxscore, *bestmove, h_n);
				}
				return maxscore;
      }
    }
    else {
      newkil = i = 0;
      maxscore = -100;
      if (look > 3)
        /* vi sorterer kun hvis der er mere end 3 treak tilbage, det     */
        /* viste sig ved forsoeg, at det i gennemsnit ikke kunne betale  */
        /* sig at sortere naar der var mindre end 3 treak tilbage        */
/*				if (look > 5)
					esort(player,&list,bd);
				else  */
			  	simsort(killer,player,bd,&list);

      do {
        trek = list.move[i++];
        /* naeste treak */
        if ((char)i < list.nmoves) {
          /* hvis vi ikke er ved det sidste mulige treak */
          newbd = *bd; 

          /* proev treakket og lav en liste af lovlig treak for   */
          /* modstanderen, kald den hurtige algoritme hvis muligt */

          if (look <= 1) {
            ftrymove(trek,player,&newbd);
            score = -slutmax3(opp,&newbd,-beta,-alfa);
            junk = 0;
          }
          else {
            trymove(trek,player,&newbd);
            score = -slutmax1(opp,look,depth + 1,&newbd,-beta,-alfa,&junk,NEWKIL);
          }
        }
        else {
          /* hvis det er sidste treak, saa behoeves vi ikke at lave en */
          /* kopi af stillingen, da vi ikke skal bruge den mere        */
          if (look <= 1) {
            ftrymove(trek,player,bd);
            score = -slutmax3(opp,bd,-beta,-alfa);
            junk = 0;
          }
          else {
            trymove(trek,player,bd);
            score = -slutmax1(opp,look,depth + 1,bd,-beta,-alfa,&junk,NEWKIL);
          }
        }
        if (score > maxscore) {
          /* hvis bedste treak, opdater scoren og vaerdier */
					if (score >= beta)
						bf = HI_HASH;
					else if (score > alfa)
						bf = OK_HASH;
					else
						bf = LO_HASH;

          if (score > alfa)
            alfa = score;
          maxscore = score;
          *bestmove = trek;
        } else /* vi opdaterer kun killer treak naar vi har et cutoff */
          if (((char)junk != 0) && ((char)junk != (char)newkil)) {
            /* vi bruger her en short int til at opbevare 2 treak. */
            newkil <<= 8;
            newkil |= junk;
          }
        if (tid_udlobet)
        /* her springer vi ud fra soegningen, hvis tiden er overskredet */
        /* for meget.                                                   */
          longjmp(env, 1);
      }
      while (((char)i < list.nmoves) && (maxscore < beta));
      /* indtil ikke flere treak eller cutoff */
			
			IF_END_HASH_PUT (depth) 
				f_hash_put (look + 1, bf, maxscore, *bestmove, h_n);

    }
  }
  else { /* ingen lovlige treak */
    *bestmove = 0;
    if (makelist(&list, opp, bd) == 0) {
      /* modstanderen har heller ingen lovlige treak, saa */
      /* spillet er slut                                  */
      aeval++;
      return (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
    }
    else
      return -slutmax1(opp,look,depth,bd,-beta,-alfa,&junk,0);
      /* vi kalder slutmin1 for at minimere */
  }
  return maxscore;
}


/**************************************************************/
/* slutmax er den variant der bruges, naar vi er ved knuder   */
/* der findes i det spiltrae som gemmes.                      */
/* Her skal treakkene ikke sorteres, da de allerede er blevet */
/* det ved den normale alfa-beta algoritme                    */
/**************************************************************/

char slutmax(short int player,
						 short int look,
             short int depth,
             board *bd,
             char alfa,
             char beta,
             short int *bsmove,
             tree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int junk,newkil,hm,opp;
  char maxscore,score,bf = HI_HASH;
	hash_num h_n;
  tree *temp;

  aknud++;
  temp = (*spiltree);
  /* temp peger pa den foerste knude, af soeskende knuderne */
	
	opp = other(player);

  if (temp->move != 0) {
    /* hvis der er lovlige traek */
		
		IF_END_HASH_GET (depth) {
      char hf;
      short int hv;
			

			f_hash_board (player, &h_n, bd);
      if ((hm = f_hash_get ((char)look, &hf, &hv, h_n)))
				if (f_legalmove (player,hm, bd)) {
					switch (hf) {
					case LO_HASH:
						if (hv >= alfa)
							goto NEXT;
						break;
					case HI_HASH:
						if (hv < beta)
							goto NEXT;
						break;
					case XX_HASH:
						goto NEXT;
					}
					if (temp->move != hm) {
						do
							temp = temp->sosk;
						while (temp->move != hm);
						put_in_front(spiltree, &temp);
					}
					*bsmove = hm;
					return (char)hv;
				} 
				else
					hm = 0;
		}
			else
				hm = 0;

		NEXT:;
		
    newkil = 0;
    maxscore = -100;
    do {
      newbd = *bd;
      trymove(temp->move, player,&newbd);
			if (depth == 0)
				make_try(temp->move,allway + 1);
      /* proev treakket */
      if (look == 0) {
        /* hvis vi er ved en slutknude saa beregn scoren */
        aeval++;
        aknud++;
        score = (char)(newbd.ndiscs[player] - newbd.ndiscs[opp]);
      }
      else {
        /* hvis ikke en slut knude fortsaettes med kald af  */
        /* enten slutmin eller slutmin1. Slutmin kaldes,    */
        /* hvis det nuvaerende treak har nogle boern, og    */
        /* ellers beregnes de mulige treak for modstanderen */
        /* og slutmin1 kaldes                               */
        if (temp->barn != NULL)
          score = -slutmax(opp,look - 1,depth + 1,&newbd,-beta,-alfa,&junk,&temp->barn);
        else 
          score = -slutmax1(opp,look - 1,depth + 1,&newbd,-beta,-alfa,&junk,NEWKIL);
       }
      if (score > maxscore) {
        /* hold rede paa den bedste score og det bedste treak */
        *bsmove = temp->move;
				if (score >= beta)
					bf = HI_HASH;
				else if (score > alfa)
					bf = OK_HASH;
				else
					bf = LO_HASH;
        if (score > alfa) {
          alfa = score;
          put_in_front(spiltree, &temp);
          /* hvis vi har et nyt bedste treak, saa saet der foerst */
          /* i vores liste af soeskende. Det er kun nodvendigt    */
          /* at goere det, hvis vi har et globalt bedste treak,   */
          /* da vi kun er intereseret i den bedste linie, naar vi */
          /* naar til slutspilsalgoritmen, da vi ikke mere soeger */
          /* ved iterativ uddybning, men helt til enden.          */
        }
        maxscore = score;
      }
      else /* vi opdaterer kun killer treak naar vi har et cutoff */
        if ((junk != 0) && ((char)junk != (char)newkil)) {
          /* vi bruger her en short int til at opbevare 2 treak. */
          newkil <<= 8;
          newkil |= junk;
        }
      if (depth == 0) {
        /* hvis vi er ved dybde 0, da opdateres nogle globale */
        /* variable, sadan vi kan foelge hvordan der soeges   */
        /* uden at skulle vente paa at computeren er faerdig  */
        /* med at taenke                                      */
        sofar++;
        bvalue = maxscore;
        if (low_min)
          b1calc = TRUE;
        else
          bcalc = TRUE;
        search_time = clock();
				search_time -= gam_tid;
				make_res(bvalue, search_time);
				if (timeout()) /* er tiden brugt op */
				break;
      }
      temp = temp->sosk;
      /* naeste treak */
      if (tid_udlobet)
        /* her springer vi ud fra soegningen, hvis tiden er overskredet */
        /* for meget.                                                   */
        longjmp(env, 1);
    }
    while ((temp != NULL) && (maxscore < beta));
    /* indtil ikke flere treak eller cutoff */
		
		 IF_END_HASH_PUT (depth) 
				f_hash_put ((char)look, (char)bf, maxscore, *bsmove, h_n);

  }
  else { /* ingen lovlige treak */
    *bsmove = 0;
    if (temp->barn != NULL)
      return -slutmax(opp,look,depth + 1,bd,-beta,-alfa,&junk,&temp->barn);
      /* der er boern, saa vi kalder slutmin */
    else {
      if (makelist(&newlist, opp, bd) == 0) {
        /* modstanderen har heller ingen lovlige treak, saa */
        /* spillet er slut                                  */
        aeval++;
        return (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
      }
      return -slutmax1(opp,look,depth,bd,-beta,-alfa,&junk,0);
      /* der er ingen boern, saa vi kalder slutmin  */
      /* look beholder sin vaerdi, da vi skal fylde */
      /* breaddet med brikker.                      */
    }
  }
  return maxscore;
}

/*****************************************************************/
/* zeroslutmax er en modificeret alfa-beta algortime, der naar   */
/* det foerste treak er beregnet fortsaetter med at soege med et */
/* snaevert alfa-beta vindue. Dette giver en besparelse i        */
/* antallet af undersoegte knuder, da vi hurtigere faar et cut   */
/* Hvis et treak findes der er bedre en det hidtil bedste, da    */
/* skal dette treak undersoeges igen med et stort alfa-beta      */
/* vindue. Da vi kun bruger zero algoritmen ved de knuder der    */
/* er beregnet og sorteret fra den normale alfa-beta algoritme   */
/* er der en stor sandsynlighed for at det foerste treak er det  */
/* bedste, og al i alt giver denne modifikation en besparelse    */
/* paa mellem 10-30 % i antallet af undersoegte knuder.          */
/*****************************************************************/

char zero_slutmax1(short int player,
             short int look,
             short int depth,
             board *bd,
             char alfa,
             char beta,
             short int *bsmove,
             tree* *spiltree);

char zero_slutmax(short int player,
             short int look,
             short int depth,
             board *bd,
             char alfa,
             char beta,
             short int *bsmove,
             tree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int junk,newkil,hm,bf,opp;
  char maxscore,score,oldbeta,window;
	hash_num h_n;
  tree *temp;


  aknud++;
  temp = (*spiltree);
  /* temp peger pa den foerste knude, af soeskende knuderne */
	
	opp = other(player);

  if (temp->move != 0) {
    /* hvis der er lovlige traek */
		
		IF_END_HASH_GET (depth) {
      char hf;
      short int hv;
			

			f_hash_board (player, &h_n, bd);
      if ((hm = f_hash_get ((char)look, &hf, &hv, h_n)))
				if (f_legalmove (player,hm, bd)) {
					switch (hf) {
					case LO_HASH:
						if (hv >= alfa)
							goto NEXT;
						break;
					case HI_HASH:
						if (hv <= beta)
							goto NEXT;
						break;
					case XX_HASH:
						goto NEXT;
					}
					if (temp->move != hm) {
						do
							temp = temp->sosk;
						while (temp->move != hm);
						put_in_front(spiltree, &temp);
					}
					*bsmove = hm;
					return (char)hv;
				} 
				else
					hm = 0;
		}
			else
				hm = 0;

		NEXT:;


    newkil = 0;
    oldbeta = beta;
		maxscore = -100;

    window = 1;
    /* window er enn boolsk variabel, der afgoer hvordan der skal    */
    /* soeges, hvis window er 0, da soeges med normal alfa-beta      */
    /* algoritme, da alfa-beta winduet saa er mindst muligt, og zero */
    /* algoritmen ikke kan indsnaevre det mere. Hhvis window er 1,   */
    /* har vi et stort alfa-beta vindue og vi skal saa bruge zero-   */
    /* window algoritmen for at faa det indsnaevret                  */


    do {
      newbd = *bd;
      trymove(temp->move, player,&newbd);
			if (depth == 0)
				make_try(temp->move,allway + 1);
      /* proev treakket */

      if (look == 0) {
        /* hvis vi er ved en slutknude saa beregn scoren */
        aeval++;
        aknud++;
        return (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
      }
      else {
        if (temp->barn != NULL)
          /* hvis vi stadig er ved en knude vi har gemt i vores trae */
          /* og alfa-beta vinduet skal indsnaevres saa kaldes zero_  */
          /* slutmin, ellers kaldes den normale alfa-beta algoritme  */
          /* slutmin                                                 */
          if (window == 1)
            score = -zero_slutmax1(opp,look - 1,depth + 1,&newbd,-beta,-alfa,&junk,&temp->barn);
          else
            score = -slutmax(opp,look - 1,depth + 1,&newbd,-beta,-alfa,&junk,&temp->barn);
        else 
          /* der er ikke gemt flere knuder, saa vi laver en liste af */
          /* lovlige treak, og kalder slutmin1                       */
          score = -slutmax1(opp,look - 1,depth + 1,&newbd,-beta,-alfa,&junk,NEWKIL);

      }
      if (window == 1) {
        /* vi har et stort alfa-beta vindue, saa vaerdien er praecis */
				if (score >= beta)
					bf = HI_HASH;
				else if (score > alfa)
					bf = OK_HASH;
				else
					bf = LO_HASH;

        if (score > alfa)
          alfa = score;
        beta = alfa + 1;
        /* her saettes beta til alfa + 1, saa vi nu har det mindst */
        /* mulige alfa-beta vindue                                 */

        /* vi ved, at treakket er det hidtil bedste, da det enten  */
        /* var det foerste treak, eller et treak der foerte til at */
        /* alfa-beta vinduet blev gjordt stort igen                */
        *bsmove = temp->move;
        put_in_front(spiltree, &temp);
        maxscore = score;
        window = 0;
        /* window saettes til 0 for at indikere at vi nu soeger med */
        /* snaevert alfa-beta vindue                                */
      }
      else if (score > maxscore) {
        /* her soeges med snaevert vindue */

        *bsmove = temp->move;
				if (score >= beta)
					bf = HI_HASH;
				else if (score > alfa)
					bf = OK_HASH;
				else
					bf = LO_HASH;
				maxscore = score;

        if (score > alfa) {
          /* hvis scoren er stoerre end alfa, saa er der et treak der */
          /* er bedre end det hidtil bedste, og vi skal nu have den   */
          /* noejagtige vaerdi for dette treak                        */
          alfa = score;

          if (alfa >= oldbeta) {
            /* hvis vi allerede har cutoff er der ingen grund til at */
            /* undersoege treakket igen                              */
            put_in_front(spiltree, &temp);
						IF_END_HASH_PUT (depth) 
							f_hash_put ((char)look, (char)bf, maxscore, *bsmove, h_n);

            return alfa;
          }

          if (depth == 0) {
            bvalue = alfa;
            search_time = clock();
						search_time -= gam_tid;
						make_res(maxscore, search_time);
					}

            /* opdaterine af global variabel, saa det ses at et bedre   */
            /* treak er fundet                                          */
          beta = oldbeta;
          /* vinduet saettes til det maximale */
          window = 1;
          /* og vi bruger zero algoritmen igen */

          continue;
          /* vi springer op til starten af loekken (ikke saerligt over- */
          /* skueligt, men "c" tillader den slags ting, der goer livet  */
          /* lidt lettere                                               */
        }
      }
      else /* killer moves som i de andre slutspils routiner */
        if ((junk != 0) && ((char)junk != (char)newkil)) {
          newkil <<= 8;
          newkil |= junk;
        }
      if (depth == 0) {
        /* globale variable opdateres, og der undersoeges for timeout */
        sofar++;
        bvalue = maxscore;
        if (low_min)
          b1calc = TRUE;
        else
          bcalc = TRUE;
        search_time = clock();
       	search_time -= gam_tid;
	  		make_res(maxscore, search_time);

        if (timeout())
          break;
      }
      temp = temp->sosk;
      /* naeste treak */
      if (tid_udlobet)
        /* her springer vi ud fra soegningen, hvis tiden er overskredet */
        /* for meget.                                                   */
        longjmp(env, 1);
    }
    while ((temp != NULL) && (maxscore < oldbeta));
    /* indtil ikke flere treak eller cutoff */
		
		IF_END_HASH_PUT (depth) 
			f_hash_put ((char)look, (char)bf, maxscore, *bsmove, h_n);

  }
  else { /* ingen lovlige treak */
    *bsmove = 0;
    if (temp->barn != NULL)
      /* hvis flere knuder gemt, saa kald zero_slutmin */
      return -zero_slutmax1(opp,look,depth + 1,bd,-beta,-alfa,&junk,&temp->barn);
    else {
      if (makelist(&newlist, opp, bd) == 0) {
        /* modstanderen har heller ingen lovlige treak, saa */
        /* spillet er slut                                  */
        aeval++;
        return (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
      }
      return -slutmax1(opp,look,depth,bd,-beta,-alfa,&junk,0);
      /* der er ingen boern, saa vi kalder slutmin  */
    }
  }
  return maxscore;
}

/* zero_slutmin er som slutmin, blot kaldes zero_slutmax, istedet for */
/* slutmax naar der er flere knuder i det spiltrae som gemmes         */
char zero_slutmax1(short int player,
             short int look,
             short int depth,
             board *bd,
             char alfa,
             char beta,
             short int *bsmove,
             tree* *spiltree)
{
  movelist newlist;
  board newbd;
  short int junk,newkil,hm,bf,opp;
  char maxscore,score;
	hash_num h_n;
  tree *temp;

  aknud++;
  temp = (*spiltree);
	opp = other(player);

  if (temp->move != 0) {
	
		IF_END_HASH_GET (depth) {
      char hf;
      short int hv;
			

			f_hash_board (player, &h_n, bd);
      if ((hm = f_hash_get ((char)look, &hf, &hv, h_n)))
				if (f_legalmove (player,hm, bd)) {
					switch (hf) {
					case LO_HASH:
						if (hv >= alfa)
							goto NEXT;
						break;
					case HI_HASH:
						if (hv <= beta)
							goto NEXT;
						break;
					case XX_HASH:
						goto NEXT;
					}
					if (temp->move != hm) {
						do
							temp = temp->sosk;
						while (temp->move != hm);
						put_in_front(spiltree, &temp);
					}
					*bsmove = hm;
					return (char)hv;
				} 
				else
					hm = 0;
		}
			else
				hm = 0;

		NEXT:;

    newkil = 0;
    maxscore = -100;
    do {
      newbd = *bd;
      trymove(temp->move,player,&newbd);
      if (look == 0) {
        aeval++;
        aknud++;
        return  (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
      }
      else {
        if (temp->barn != NULL)
          score = -zero_slutmax(opp,look - 1,depth + 1,&newbd,-beta,-alfa,&junk,&temp->barn);
        else 
          score = -slutmax1(opp,look - 1,depth + 1,&newbd,-beta,-alfa,&junk,NEWKIL);
       }
      if (score > maxscore) {
				if (score >= beta)
					bf = HI_HASH;
				else if (score > alfa)
					bf = OK_HASH;
				else
					bf = LO_HASH;

        *bsmove = temp->move;
        if (score > alfa) {
          alfa = score;
        	put_in_front(spiltree, &temp);
        }
        maxscore = score;
      }
      else
        if ((junk != 0) && ((char)junk != (char)newkil)) {
          newkil <<= 8;
          newkil |= junk;
        }
      temp = temp->sosk;
      if (tid_udlobet)
        longjmp(env, 1);
    }
    while ((temp != NULL) && (maxscore < beta));
		
		IF_END_HASH_PUT (depth) 
			f_hash_put ((char)look, (char)bf, maxscore, *bsmove, h_n);

  }
  else {
    *bsmove = 0;
    if (temp->barn != NULL)
      return -zero_slutmax(opp,look,depth + 1,bd,-beta,-alfa,&junk,&temp->barn);
    else {
      if (makelist(&newlist, opp, bd) == 0) {
        aeval++;
        return (char)(bd->ndiscs[player] - bd->ndiscs[opp]);
      }
      return -slutmax1(opp,look,depth,bd,-beta,-alfa,&junk,0);
    }
  }
  return maxscore;
}
