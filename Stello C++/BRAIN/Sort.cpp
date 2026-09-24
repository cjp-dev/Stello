/***************************************************************************/
/*                                                                         */
/*                 sort.c,     Sorteringsalgoritmer                        */
/*                                                                         */
/***************************************************************************/
#include "stdafx.h"
#include "reversi.h" 
#include <limits.h>

/*****************************************************************/
/* humres og comres indeholde listerne af gode svar for de to    */
/* spillere. Stoerrelsen er minimeret til 78, ved at indexere    */
/* med - 11. Saledes svarer treak 11 til 0 i vores array, og vi  */
/* kan slaa op direkte ved at bruge traekket. Ligeledes er det   */
/* kun noedvendigt at gaa op til 77 = 88 - 11.                   */
/*****************************************************************/

extern int humres[78][78],comres[78][78];

/*****************************************************************/
/* disse vaerdier bruges til at sortere de treak som ikke bliver */
/* sorteret efter "response killer" eller "killer sorteringen    */
/* Metoden baserer sig paa, at nogle felter er bedre end andre   */
/* Selve vaerdierne bliver opdateret under soegningen, saaledes, */
/* at hvis et hjoerne er optaget, saa er det godt at saette en   */
/* brik taet ved dette hjoerne, hvis farver paa hjoernebrikken   */
/* er den samme som ens egen farve                               */
/*****************************************************************/

char scores[100] = {-100,  0,  0,  0,  0,  0,  0,  0,  0,  0,
                       0,127,-32, 22, 11, 11, 22,-32,127,  0,
                       0,-32,-64, -8, -5, -5, -8,-64,-32,  0,
                       0, 22, -8, 12,  1,  1, 12, -8, 22,  0,
                       0, 11, -5,  1,  0,  0,  1, -5, 11,  0,
                       0, 11, -5,  1,  0,  0,  1, -5, 11,  0,
                       0, 22, -8, 12,  1,  1, 12, -8, 22,  0,
                       0,-32,-64, -8, -5, -5, -8,-64,-32,  0,
                       0,127,-32, 22, 11, 11, 22,-32,127,  0,
                       0,  0,  0,  0,  0,  0,  0,  0,  0,  0};

/****************************************************************/
/* simsort bruges til at sortere treakkene under slutspils-     */
/* algoritmen, da response killer metoden ikke virker saa godt  */
/* her. Dette skyldes, at under selve slutspillet er mange      */
/* ikke intuitive forstaaelige, da selve spillet her ofte ud-   */
/* vikler sig til regn og skaer kombinatorik, hvor det som      */
/* er bedre at anvende killer sortering, hvor treakkene er mere */
/* lokalt bestemt, fremfor "response killer" sorteringen hvor   */
/* treakkene har mere global karakterer.                        */
/****************************************************************/

void simsort(short int killer,short int player, board *bd, movelist * list)
{
  short int opp,treak,i,j,temp1;
  char *lpoi,temp;

  lpoi = list->move;
  /* lpoi peger paa det foerste treak */

  treak = list->nmoves;
  /* treak er antallet ikke sorterede treak */

  while (killer != 0 && treak > 1) {
  /* killer treakkene flyttes frem forest i listen, hvis de */
  /* findes i listen                                        */
    for (i = 0; i < treak; i++)
      if (lpoi[i] == (char)killer) {
        /* killer treak er fundet */
        lpoi[i] = lpoi[0];
        lpoi[0] = (char)killer;
        treak--;
        /* et treak mindre at sortere */
        lpoi++;
        /* flyt pointer frem til naeste treak i listen */
        break;
      }
    killer >>= 8;
    /* naeste killer treak */
  }

  if (treak <= 1) /* hvis mindre en et treak tilbage saa retuner */
    return;
  opp = other(player);

  /* her opdateres vaerdierne taet ved hjoernerne dynamisk */
  /* for at faa en bedre sortering                         */

  if (bd->sq[11] == (char)player) {
    scores[12] = 34;
    scores[21] = 34;
    scores[22] = 24;
  } else if (bd->sq[11] == (char)opp) {
    if (bd->sq[13] == (char)opp)
      scores[12] = 32;
    else
      scores[12] = -32;
    if (bd->sq[31] == (char)opp)
      scores[21] = 32;
    else
      scores[21] = -32;
    scores[22] = -16;
  } else {
    scores[12] = -16;
    scores[21] = -16;
    scores[22] = -64;
  }
  if (bd->sq[18] == (char)player) {
    scores[17] = 34;
    scores[28] = 34;
    scores[27] = 24;
  } else if (bd->sq[18] == (char)opp) {
    if (bd->sq[16] == (char)opp)
      scores[17] = 32;
    else
      scores[17] = -32;
    if (bd->sq[38] == (char)opp)
      scores[28] = 32;
    else
      scores[28] = -32;
    scores[27] = -16;
  } else {
    scores[17] = -16;
    scores[28] = -16;
    scores[27] = -64;
  }
  if (bd->sq[81] == (char)player) {
    scores[71] = 34;
    scores[82] = 34;
    scores[72] = 24;
  } else if (bd->sq[81] == (char)opp) {
    if (bd->sq[61] == (char)opp)
      scores[71] = 32;
    else
      scores[71] = -32;
    if (bd->sq[83] == (char)opp)
      scores[82] = 32;
    else
      scores[82] = -32;
    scores[72] = -16;
  } else {
    scores[71] = -16;
    scores[82] = -16;
    scores[72] = -64;
  }
  if (bd->sq[88] == (char)player) {
    scores[87] = 34;
    scores[78] = 34;
    scores[77] = 24;
  } else if (bd->sq[88] == (char)opp) {
    if (bd->sq[86] == (char)opp)
      scores[87] = 32;
    else
      scores[87] = -32;
    if (bd->sq[68] == (char)opp)
      scores[78] = 32;
    else
      scores[78] = -32;
    scores[77] = -16;
  } else {
    scores[87] = -16;
    scores[78] = -16;
    scores[77] = -64;
  }

  /* sorter resten af treakkene efter feltvvaerdierne  */
  /* der bruges en variant af linaersorterings metoden */
  /* da denne viste sig at vaere den hurtigste til at  */
  /* sortere arrays med mellem 2 og 25 elementer       */

  lpoi[treak] = 0;
  for (i = treak-2; i>=0; i--) {
    temp1 = lpoi[i];
    temp = scores[temp1];
    for (j=i+1; (temp < scores[(short int)lpoi[j]]); j++)
      lpoi[j-1] = lpoi[j];
    lpoi[j-1] = (char)temp1;
  }
}

/****************************************************************/
/* sortlist bruges til at sortere treakkene under den normale   */
/* alfa-beta algoritme. Foerst sorteres efter responsekiller    */
/* vaerdierne, og de treak for hvilke der ingen vaerdier var,   */
/* sorteres efter faeltvaerdierne                               */
/****************************************************************/

void sortlist(short int killer,short int pmove, short int player, movelist *list, board *bd)
{
  short int treak,opp,i,j,temp1;
  char *lpoi,temp;
  int *rscores;
	
	lpoi = list->move;
  treak = list->nmoves;

	
	if (killer != 0 ) {
  /* killer treakkene flyttes frem forest i listen, hvis de */
  /* findes i listen                                        */
    for (i = 0; i < treak; i++)
      if (lpoi[i] == (char)killer) {
        /* killer treak er fundet */
        lpoi[i] = lpoi[0];
        lpoi[0] = (char)killer;
        treak--;
        /* et treak mindre at sortere */
        lpoi++;
        /* flyt pointer frem til naeste treak i listen */
        break;
      }
  }


  if (pmove != 0) {
    /* hvis pmove er forskellig fra 0, da har modstanderen */
    /* foretaget et treak i forrige niveay af spiltreaet   */

    /* rscores er en pointer paa de respektive gode svartreak */
    /* for henholdsvis computeren og modstanderen. Der er 2   */
    /* arrays af lister, da det jo ikke er sikket at et godt  */
    /* svar for sort ogsaa er et godt svar for hvid           */

    if (player == computer)
      rscores = comres[pmove - 11];
    else
      rscores = humres[pmove - 11];

    rscores[19-11] = INT_MIN;

    /* her sorteres treakkene efter response killer vaerdierne */
    /* linaersorterings metoden */

    lpoi[treak] = 19;
    for (i = treak-2; i>=0; i--) {
      temp1 = lpoi[i];
      temp = rscores[temp1 - 11];
      for (j=i+1; (temp < rscores[lpoi[j] - 11]); j++)
        lpoi[j-1] = lpoi[j];
      lpoi[j-1] = (char)temp1;
    }

    /* vi finder her det foerste treak hvorom det gaelder, at  */
    /* der ingen resultater var fra response killer vaerdierne */
    /* alle treak herefter er ikke sorteret, og bliver nu      */
    /* sorteret efter feltvaerdierne.                          */

    while (treak >= 2)
      if (!rscores[*lpoi])
        break;
      else {
        lpoi++;
        treak--;
      }
  } 
  if (treak <= 1) /* hvis mindre en et treak tilbage saa retuner */
    return;
  opp = other(player);

  /* her opdateres vaerdierne taet ved hjoernerne dynamisk */
  /* for at faa en bedre sortering                         */

  if (bd->sq[11] == (char)player) {
    scores[12] = 34;
    scores[21] = 34;
    scores[22] = 24;
  } else if (bd->sq[11] == (char)opp) {
    if (bd->sq[13] == (char)opp)
      scores[12] = 32;
    else
      scores[12] = -32;
    if (bd->sq[31] == (char)opp)
      scores[21] = 32;
    else
      scores[21] = -32;
    scores[22] = -16;
  } else {
    scores[12] = -16;
    scores[21] = -16;
    scores[22] = -64;
  }
  if (bd->sq[18] == (char)player) {
    scores[17] = 34;
    scores[28] = 34;
    scores[27] = 24;
  } else if (bd->sq[18] == (char)opp) {
    if (bd->sq[16] == (char)opp)
      scores[17] = 32;
    else
      scores[17] = -32;
    if (bd->sq[38] == (char)opp)
      scores[28] = 32;
    else
      scores[28] = -32;
    scores[27] = -16;
  } else {
    scores[17] = -16;
    scores[28] = -16;
    scores[27] = -64;
  }
  if (bd->sq[81] == (char)player) {
    scores[71] = 34;
    scores[82] = 34;
    scores[72] = 24;
  } else if (bd->sq[81] == (char)opp) {
    if (bd->sq[61] == (char)opp)
      scores[71] = 32;
    else
      scores[71] = -32;
    if (bd->sq[83] == (char)opp)
      scores[82] = 32;
    else
      scores[82] = -32;
    scores[72] = -16;
  } else {
    scores[71] = -16;
    scores[82] = -16;
    scores[72] = -64;
  }
  if (bd->sq[88] == (char)player) {
    scores[87] = 34;
    scores[78] = 34;
    scores[77] = 24;
  } else if (bd->sq[88] == (char)opp) {
    if (bd->sq[86] == (char)opp)
      scores[87] = 32;
    else
      scores[87] = -32;
    if (bd->sq[68] == opp)
      scores[78] = 32;
    else
      scores[78] = -32;
    scores[77] = -16;
  } else {
    scores[87] = -16;
    scores[78] = -16;
    scores[77] = -64;
  }

  /* sorter resten af treakkene efter feltvvaerdierne */
  /* linaersorterings metoden */

  lpoi[treak] = 0;
  for (i = treak-2; i>=0; i--) {
    temp1 = lpoi[i];
    temp = scores[temp1];
    for (j=i+1; (temp < scores[(short int)lpoi[j]]); j++)
      lpoi[j-1] = lpoi[j];
    lpoi[j-1] = (char)temp1;
  }

}

short int dangerous(short int move, char pl, board *bd);

void esort(short int player, movelist *list, board *bd)
{
  short int treak,trek,opp,i,j,temp1;
  char *lpoi,temp;
  board newbd;
	int escores[100];
	
		i = 0;
		opp = other(player);
	  do {
      trek = list->move[i++];
      /* naeste mulige treak */
      newbd = *bd;
      trymove(trek, player,&newbd);
        /* udfoer dette treak */

 /*     if (!dangerous(trek, player,&newbd)) { */
        /* hvis vi er ved en slutstilling, og denne ikke er "urolig" */
        /* saa evaluer denne                                         */
				if (player == computer)
        	escores[trek] = -eval(-32665, 32665,list->nmoves, opp,&newbd);
				else
					escores[trek] = -eval(32665, -32665,list->nmoves, opp,&newbd);
 /*     }
      else {
        makelist(&newlist,opp,&newbd);
				if (player == computer)
					escores[trek] = -findmax2(opp,0,64,0,&newlist,&newbd,32100, -32100,&junk);
				else
					escores[trek] = -findmax2(opp,0,64,0,&newlist,&newbd,-32100, 32100,&junk);
      } */
    } 
    while (((char)i < list->nmoves));

	
	lpoi = list->move;
  treak = list->nmoves;

	
  /* sorter resten af treakkene efter feltvvaerdierne */
  /* linaersorterings metoden */

  lpoi[treak] = 0;
	escores[0] = INT_MIN;
  for (i = treak-2; i>=0; i--) {
    temp1 = lpoi[i];
    temp = escores[temp1];
    for (j=i+1; (temp < escores[(short int)lpoi[j]]); j++)
      lpoi[j-1] = lpoi[j];
    lpoi[j-1] = (char)temp1;
  }

}
