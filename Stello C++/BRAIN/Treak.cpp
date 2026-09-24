/*************************************************************************/
/*                                                                       */
/*      treak.c,  algoritmer til generering af treak og stillinger       */
/*                                                                       */
/*************************************************************************/
#include "stdafx.h"
#include <stdio.h>
#include "reversi.h"

short int delta[8];
board  mainboard;
extern board play_bd,color_bd;
movelist list;
short int playnm;
short int lookahead,gameover;
extern short int movenum;

tree *blackroot,*whiteroot;

int inlist(short int b, movelist *l)
{    
  short int z;

  for (z = 0; (char)z < l->nmoves; z++)
    if (l->move[z] == (char)b) 
      return TRUE;
  return FALSE;
}

/********************************************************/
/* add move tilfoejer et treak til listen hvis det ikke */
/* allerede findes                                      */
/********************************************************/

void addmove(short int k, movelist *list)
{
  short int i;

  list->move[(short int)list->nmoves] = (char)k;
  /* saet treakket i slutningen af listen */

  i = 0;
  while (list->move[i] != (char)k) i++;
  /* find det foerste treak i listen der er lig treakket k */

  if ((char)i == list->nmoves) list->nmoves++;
  /* hvis vi er ved slutningen af listen fandtes treakket ikke */
  /* i listen i forvejen, og vi opdaterer antallet af treak    */
}

/***************************************/
/* delmove sletter et treak fra listen */
/***************************************/

void delmove(short int k, movelist *list)
{
  short int i;

  list->move[(short int)list->nmoves] = (char)k;
  /* saet treakket i slutningen af listen */

  i = 0;
  while (list->move[i] != (char)k) i++;
  /* find det foerste treak i listen der er lig treakket k */

  if ((char)i < list->nmoves) {
    /* hvis vi ikke er ved slutningen af listen fandtes treakket */
    /* i listen i forvejen, og vi opdaterer antallet af treak    */
    list->nmoves--;

    list->move[i] = list->move[(short int)list->nmoves];
    /* vi flytter det sidste treak i listen hen paa den plads  */
    /* hvor det treak der skal slettes staar                   */
  }
}

/********************************************************************/
/* flanking undersoeger om et treak er tilladt i en bestemt retning */
/********************************************************************/

short int flanking(short int k, short int dir, board *bd, short int pl)
{
  short int del,opponent;

  opponent = other(pl);
  del = delta[dir];
  k += del;
  if (bd->sq[k] == (char)opponent) {
    /* den foerste brik i retningen skal vaere en modstander brik */

    /* vi overspringer alle andre modstander brikker */
    do k += del;
    while (bd->sq[k] == (char)opponent);

    /* vi retuneren om det naeste felt har en brik af samme farve */
    /* som spilleren, hvis dette er sandt saa er treakket lovlig  */
    /* i denne retning                                            */
    return (bd->sq[k] == (char)pl);
  }
  else
    return FALSE;
    /* den foerste brik i retningen var ikke en modstander brik */
    /* saa der kunne ikke vendes brikker i denne retning        */
}

/********************************************************************/
/* flan1king ligesom flanking, bruges til profilering af programmet */
/* saaledes at man kan adskille de algoritmer der bruges til        */
/* evaluering og til traekgenerering                                */
/********************************************************************/

short int flan1king(short int k, short int dir, board *bd, short int pl)
{
  short int del,opponent;

  opponent = other(pl);
  del = delta[dir];
  k += del;
  if (bd->sq[k] == (char)opponent) {
    do k += del;
    while (bd->sq[k] == (char)opponent);
    return (bd->sq[k] == (char)pl);
  }
  else
    return FALSE;
}

#ifndef FAST
/***************************************************************/
/* her er de overskuelige algoritmer til generering af mulige  */
/* treak, og til generering af nye stillinger                  */
/***************************************************************/

/***********************************************/
/* legalmove undersoger om et treak er lovligt */
/***********************************************/

short int legalmove(short int k, board *bd, short int pl)
{
  short int dir;

  /* vi undersoeger om et treak er lovligt, ved for hver retning */
  /* at undersoege om der kan vendes brikker i denne retning     */
  /* lige saa snart en retning er fundet saa ved vi at treakket  */
  /* er lovlig, og vi kan retunere SAND                          */
  for (dir = NORTH; dir <= NORTHWEST; dir++)
    if (flanking(k, dir, bd, pl))
      return TRUE;

  /* Der kunne ikke vendes brikker i nogen retning, saa vi */
  /* retunerer FALSK                                       */
  return FALSE;
}

/******************************************************************/
/* makelist tager en stilling og retunerer listen af mulige treak */
/* for spilleren pl i listen legal, og antaller af treak som      */
/* funktionsvaerdi                                                */
/******************************************************************/

char makelist(movelist *legal, short int pl, board *bd)
{
  short int i;

  legal->nmoves = 0;

  for (i = 0; (char)i < bd->possible.nmoves; i++) {
  /* for hvert muligt treak undersoeges om det er lovligt */
    if (legalmove(bd->possible.move[i], bd, pl)) {
      /* hvis det er lovligt tilfoejes det til listen af lovlige */
      /* treak, og antallet af treak opdateres                   */
      legal->move[legal->nmoves] = bd->possible.move[i];
      legal->nmoves ++;
      }
  }
  /* antallet af treak retuneres */
  return legal->nmoves;
}

/*******************************************************************/
/* countmov bruges af evaluerings funktionen, den taeller antallet */
/* af lovlige treak for spilleren pl, og taeller antallet af       */
/* potientielle veje til tomme felter for begge spillerer          */
/*******************************************************************/

short int countmov(short int pl, board *bd, short int potential[])
{
  short int del,opponent,moves,trek,i,dir;

  opponent = other(pl);
  moves = 0;
  potential[0] = 0;
  potential[1] = 0;
  for (i = 0; (char)i < bd->possible.nmoves; i++) {
    /* for hvert tomt felt op til en brik undersoeges */
    trek = bd->possible.move[i];

    /* her om det er et lovligt treak for spilleren pl */
    for (dir = NORTH; dir <= NORTHWEST; dir++) {
      del = delta[dir];
      /* delta er et array med de vaerdier der skal laegges til   */
      /* et felt for at faa indexet i helholdsvis lodret, vandret */
      /* og diagonal retning
                                           */
      if (flan1king(trek,dir,bd,pl)) {
        moves++;
        break;
      }
    }

    /* og her findes de potientielle adgangsveje til tomme felter */
    /* for begge spillere. Det ses, at der virkelig kun findes    */
    /* legale adgangsveje, da det undersoges om feltet 2 skridt   */
    /* vaek fra det tomme felt er inden for selve breaddet, hvis  */
    /* ikke, saa kan feltet ikke naaes fra den retning med et     */
    /* lovligt treak                                              */
    for (dir = NORTH; dir <= NORTHWEST; dir++) {
      del = delta[dir];
      if (bd->sq[trek + del] == (char)opponent &&
          bd->sq[trek + del + del] != BORDER)
        potential[pl]++;
      else if (bd->sq[trek + del] == (char)pl &&
          bd->sq[trek + del + del] != BORDER)
        potential[opponent]++;
    }
  }
  /* antallet af treak retuneres */
  return moves;
}

/*******************************************************************/
/* trymove generer en ny stilling ud fra den gamle, selve treakket */
/* og hvilken farve spiller der udfoerer treakket har              */
/*******************************************************************/

void trymove(short int trysq, short int pl, board *bd)
{
  short int dir,k1,opp,del;

  opp = other(pl);

  bd->sq[trysq] = (char)pl;
  /* saet brikken pa breaddet */

  bd->ndiscs[pl]++;
  /* spilleren pl har faaet en brik mere */

  delmove(trysq,&bd->possible);
  /* vi sletter det tomme felt fra listen af potientielle treak */

  /* for hver retning skal der nu vendes brikker hvis muligt */
  for (dir = NORTH; dir <= NORTHWEST; dir++) {
    del = delta[dir];
    /* delta er et array med de vaerdier der skal laegges til   */
    /* et felt for at faa indexet i helholdsvis lodret, vandret */
    /* og diagonal retning                                      */

    if (flanking(trysq,dir,bd,pl)) {
      /* det var lovlig at vende brikker i denne retning */
      k1 = trysq + del;
      do {
        /* vi vender brikkerne og opdaterer antallet af brikker */
        /* for spiller og modstander                            */
        bd->sq[k1] = (char)pl;
        bd->ndiscs[pl]++;
        bd->ndiscs[opp]--;
        k1 += del;
      }
      while (bd->sq[k1] != (char)pl);
    }
    else if (bd->sq[trysq + del] == EMPTY)
      /* hvis feltet i en retning vaek fra den lige satte brik */
      /* er tomt, saa tilfoejes det til listen af potientielle */
      /* treak, hvis det ikke allerede findes der (undersoeges */
      /* i addmove)                                            */
      addmove(trysq + del,&bd->possible);
  }
}

#else

/*****************************************************************/
/* her er de uoverskuelige men hurtige algoritmer til generering */
/* af mulige treak, og til generering af nye stillinger          */
/*****************************************************************/

/******************************************************************/
/* den grundlaeggende ide i de hurtige algoritmer er, at undgaa   */
/* at undersoege om der kan vendes brikker i retninger hvor vi    */
/* paa forhaand kan sige, at det ikke er muligt. Dertil beregnes  */
/* to 2-dimensionelle arrays, et til makelist og et til trymove   */
/* Disse 2 arrays skal betragtes som et breadt, af lister. Hvis   */
/* vi feks vil undersoge om treakket paa felt 11 er et lovligt    */
/* treak, saa bruges selve treakket som index i arrayet, og det   */
/* array vi faar fat paa, indholder en liste af de retninger      */
/* hvor det teoretisk er muligt, at vende brikker. I vores        */
/* eksempel, kan det ses, at den array vi faar fat paa indeholder */
/* elementerne 1,11,10,0. 1,11 og 10 er vaerdiern for at gaa      */
/* henholdsvis vandret, diagonalt og lodret ud fra felt 11 og 0   */
/* bruges som slutmarkoer. Denne motode sparer en masse tid.      */
/* Ved trymove, kan der spares knap saa meget tid, da vi ogsaa    */
/* skal undersoege om de tomme felter op til den nye brik er i    */
/* listen af potientilelle treak, men en gevinst er der.          */
/* desuden bruges poshort intere, og de smaa funktioner er blevet       */
/* flyttet ind i selve hovedfunktionerne makelist, trymove og     */
/* countmove                                                      */
/******************************************************************/

/*************************************************/
/* dirs er retningerne for makelist og countmove */
/*************************************************/
short int dirs [89][16] = {
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{1,11,10,0},
{1,11,10,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{10,9,-1,0},
{10,9,-1,0},
{0},
{0},
{1,11,10,0},
{1,11,10,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{10,9,-1,0},
{10,9,-1,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,0},
{-10,-9,1,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-1,-11,0},
{-10,-1,-11,0},
{0},
{0},
{-10,-9,1,0},
{-10,-9,1,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-1,-11,0},
{-10,-1,-11,0},
};

/************************************/
/* dirs1 er retningerne for trymove */
/************************************/

short int dirs1 [89][16] = {
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{0},
{1,11,10,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{1,11,10,9,-1,0},
{10,9,-1,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,11,10,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,-9,1,11,10,9,-1,-11,0},
{-10,10,9,-1,-11,0},
{0},
{0},
{-10,-9,1,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-9,1,-1,-11,0},
{-10,-1,-11,0},
};

/**************************************/
/* den optimerede version af makelist */
/**************************************/

char makelist(movelist *legal, short int pl, board *bd)
{
  short int i,del,opponent,trek, *delpo;
  char *mvpo,*bradt;

  opponent = other(pl);
  legal->nmoves = 0;
  mvpo = legal->move;
  /* mvpo er en poshort inter til de lovlige treak */

  i = bd->possible.nmoves;
  /* i indeholder antallet af resterende treak. Vi starter */
  /* bagerst i listen og taeller ned, da det som regel er  */
  /* hurtigere at teste for lighed med 0, end at taelle op */
  /* og sammenligne med et tal det ligger i hukkommelsen   */

  while (--i >= 0) {
    delpo = dirs[trek = bd->possible.move[i]];
    /* delpo peger nu paa de retninger hvor det teoretisk */
    /* er muligt at vende brikker                         */

    del = *delpo++;
    /* del er nu den foerste retning  */
    do {
      bradt = (char *)(&bd->sq[trek]) + del;
      /* bradt peger nu paa det foerste felt i denne retning */

      if (*bradt == (char)opponent) {
        /* hvis dette felt har en modstander brik */
        do bradt += del;
        while (*bradt == (char)opponent);
        /* spring resten af modstanderbrikkerne over */

        if (*bradt == (char)pl) {
          /* hvis naeste felt har en spiller brik, saa er */
          /* treakket lovligt, og veardierne opdateres    */
          *mvpo++ = (char)trek;
          legal->nmoves++;
          break;
          /* vi stopper med det samme uden at undersoege */
          /* resten af retningerne, da vi allerede har   */
          /* fastslaaet, at treakket er lovligt.         */
        }
      }
    } while ((del = *delpo++) != 0);
      /* indtil ikke flere retninger */
  };
    /* og ikke flere treak */

  return legal->nmoves;
  /* retuner antallet af treak */
}

char legalmoves(short int pl, board *bd)
{
  short int i,del,opponent,trek, *delpo;
  char *bradt;

  opponent = other(pl);

  i = 1;
  /* i indeholder antallet af resterende treak. Vi starter */
  /* bagerst i listen og taeller ned, da det som regel er  */
  /* hurtigere at teste for lighed med 0, end at taelle op */
  /* og sammenligne med et tal det ligger i hukkommelsen   */

  do {
    delpo = dirs[trek = bd->possible.move[i]];
    /* delpo peger nu paa de retninger hvor det teoretisk */
    /* er muligt at vende brikker                         */

    del = *delpo++;
    /* del er nu den foerste retning  */
    do {
      bradt = (char *)(&bd->sq[trek]) + del;
      /* bradt peger nu paa det foerste felt i denne retning */

      if (*bradt == (char)opponent) {
        /* hvis dette felt har en modstander brik */
        do bradt += del;
        while (*bradt == (char)opponent);
        /* spring resten af modstanderbrikkerne over */

        if (*bradt == (char)pl) {
          /* hvis naeste felt har en spiller brik, saa er */
          /* ET  lovligt treak                           */
          return TRUE;
        }
      }
    } while ((del = *delpo++) != 0);
      /* indtil ikke flere retninger */
  } while (--i >= 0);
    /* og ikke flere treak */

  return FALSE;
  /* retuner antallet af treak */
}

char f_legalmove(short int pl, short int trek, board *bd)
{
  short int del,opponent,*delpo;
  char *bradt;

  opponent = other(pl);

    delpo = dirs[trek];
    /* delpo peger nu paa de retninger hvor det teoretisk */
    /* er muligt at vende brikker                         */

    del = *delpo++;
    /* del er nu den foerste retning  */
    do {
      bradt = (char *)(&bd->sq[trek]) + del;
      /* bradt peger nu paa det foerste felt i denne retning */

      if (*bradt == (char)opponent) {
        /* hvis dette felt har en modstander brik */
        do bradt += del;
        while (*bradt == (char)opponent);
        /* spring resten af modstanderbrikkerne over */

        if (*bradt == (char)pl) {
          /* hvis naeste felt har en spiller brik, saa er */
          /* ET  lovligt treak                           */
          return TRUE;
        }
      }
    } while ((del = *delpo++) != 0);
      /* indtil ikke flere retninger */

  return FALSE;
  /* retuner antallet af treak */
}


/**************************************/
/* den optimerede version af countmov */
/**************************************/

short int countmov(short int pl, board *bd, short int potential[])
{
  short int del,opponent,moves,trek, *delpo;
  char *mvpo,*bradt;

  opponent = other(pl);
  moves = 0;
  potential[0] = 0;
  potential[1] = 0;
  bd->possible.move[(short int)bd->possible.nmoves] = 0;
  /* pladsen efter det sidste treak bruges til at markere */
  /* slutningen af listen af treak                        */

  mvpo = (char *)bd->possible.move;
  /* mvpo er en poshort inter til de mulige treak */

  while ((trek = *mvpo++) != 0) {
    /* saa laenge der er flere treak */

    delpo = dirs[trek];
    /* delpo peger nu paa de retninger hvor det teoretisk */
    /* er muligt at vende brikker                         */

    del = *delpo++;
    /* del er nu den foerste retning  */

    do {
      bradt = (char *)(&bd->sq[trek]) + del;
      /* bradt peger nu paa det foerste felt i denne retning */

      if (*bradt == (char)opponent) {
        /* hvis dette felt har en modstander brik */

        potential[pl]++;
        /* saa er der ogsaa en potiential adgang til dette tomme */
        /* felt for spilleren                                    */

        do bradt += del;
        while (*bradt == (char)opponent);
        /* spring resten af modstanderbrikkerne over */

        if (*bradt == (char)pl) {
          /* hvis naeste felt har en spiller brik, saa er     */
          /* treakket lovligt, og antallet af treak opdateres */
          moves++;
          break;
        }
      }
      else if (*bradt == (char)pl)
        /* hvis feltet havde en modstander brik  */
        /* saa er der ogsaa en potiential adgang */
        /* til dette tomme felt for modstanderen */
        potential[opponent]++;

    } while ((del = *delpo++) != 0);
    /* her stoppes hvis der ikke er flere retninger, eller   */
    /* hvis treakket var et lovligt treak. Hvis treakket var */
    /* et lovligt treak, saa skal vi undersoege resten af    */
    /* retningerne for at faa de rigtige vaerdier for de     */
    /* potientielle adgangsveje til de tomme felter          */

    if (del != 0)
      while ((del = *delpo++) != 0) {
        /* hvis der er flere retninger */
        del += trek;
        /* del er index paa naeste felt i denne retning */

        del = (short int)bd->sq[del];
        /* del er nu vaerdien af dette felt */
        /* resten er som ovenstaaende       */
        if ((char)del == (char)opponent)
          potential[pl]++;
        else if ((char)del == (char)pl)
          potential[opponent]++;
      }
  }
  return moves;
  /* antallet af treak retuneres */
}

/********************************************/
/* her er den optimerede version af trymove */
/********************************************/

void trymove(short int trysq, short int pl, board *bd)
{
  short int k1,opp,del,*delpo,vendt;
  char *mpoi,*mslut;

  /* de naeste 4 linier sletter treakket "trysqr" fra listen af  */
  /* mulige treak, og reducerer antallet af mulige treak, det er */
  /* altsaa "delmove" der er optimeret og flyttet ind i trymove  */

  mpoi = bd->possible.move;
  /* mpoi peger paa de mulige treak */

  mslut = (mpoi + --bd->possible.nmoves);
  /* mslut peger paa det sidste mulige treak, ao antallet af */
  /* treak reduceres med 1                                   */

  while (*mpoi++ != (char)trysq);
  /* vi finder treakket i listen */

  *(mpoi - 1) = *mslut;
  /* og sletter det ved at flytte det sidste treak i listen hen */
  /* paa den plads hvor "trysq" stod                            */

  opp = other(pl);
  bd->sq[trysq] = (char)pl;
  /* vi saetter brikken paa braeddet */

  delpo = dirs1[trysq];
  /* delpo peger nu paa de retninger hvor det er teoretisk muligt */
  /* at vende brikker, plus de retninger, hvor der kan vaere et   */
  /* nyt tomt felt                                                */

  del = *delpo++;
  /* del er den foerste af disse retninger */

  vendt = 0;
  /* vi holder rede paa antallet af vendte brikker i en variabel  */
  /* saaledes, at compileren kan bruge et register til opdatering */

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
        /* rede paa hvor mange der er vendt                      */

        k1 -= del;
        /* vi skal lige et skridt tilbage, da vi peger paa den */
        /* spiller brik der bruges til at vende brikkerne med  */

        do {
          bd->sq[k1] = (char)pl;
          vendt++;
          k1 -= del;
        }
        while (k1 != trysq);
        /* vi vender brikker indtil vi er tilbage ved vores */
        /* udgangs felt                                     */
      }
    }
    else if (bd->sq[k1] != (char)pl) {
      /* hvis feltet i en retning vaek fra den lige satte brik */
      /* er tomt, saa tilfoejes det til listen af potientielle */
      /* treak, hvis det ikke allerede findes der              */

      mpoi = bd->possible.move;
      /* mpoi peger paa foerste mulige treak */

      *mslut = (char)k1;
      /* mslut peger paa foerste tomme plads i listen  */
      /* vi saetter treakket k1 paa denne plads for at */
      /* vaere sikker paa, at naeste linie standser    */

      while (*mpoi++ != (char)k1);
      /* vi loeber listen igennem for at finde treakket k1 */

      if (mpoi > mslut) {
        /* treakket var ikke allerede i listen, da vi naaede   */
        /* ud til hvor mslut pegede, saa vi opdaterer antallet */
        /* af treak i listen, og saetter mslut til at pege paa */
        /* naeste frie plads                                   */
        bd->possible.nmoves++;
        mslut++;
      }
    }
  } while ((del = *delpo++) != 0);
  /* indtil ikke flere retninger */

  bd->ndiscs[opp] -= vendt;
  /* modstanderen fik vendt "vendt" brikker */

  vendt++;
  bd->ndiscs[pl] += vendt;
  /* spilleren fik "vendt" + 1 brik, da han ogsaa satte en brik */
  /* paa breaddet                                               */
}

void ftrymove(short int trysq, short int pl, board *bd)
{
  short int k1,opp,del,*delpo,vendt;

  if (bd->possible.move[0] == (char)trysq)
    bd->possible.move[0] = bd->possible.move[2];
  else if (bd->possible.move[1] == (char)trysq)
    bd->possible.move[1] = bd->possible.move[2];

  opp = other(pl);
  bd->sq[trysq] = (char)pl;
  /* vi saetter brikken paa braeddet */

  delpo = dirs[trysq];
  /* delpo peger nu paa de retninger hvor det er teoretisk muligt */
  /* at vende brikker, plus de retninger, hvor der kan vaere et   */
  /* nyt tomt felt                                                */

  del = *delpo++;
  /* del er den foerste af disse retninger */

  vendt = 0;
  /* vi holder rede paa antallet af vendte brikker i en variabel  */
  /* saaledes, at compileren kan bruge et register til opdatering */

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
        /* rede paa hvor mange der er vendt                      */

        k1 -= del;
        /* vi skal lige et skridt tilbage, da vi peger paa den */
        /* spiller brik der bruges til at vende brikkerne med  */

        do {
          bd->sq[k1] = (char)pl;
          vendt++;
          k1 -= del;
        }
        while (k1 != trysq);
        /* vi vender brikker indtil vi er tilbage ved vores */
        /* udgangs felt                                     */
      }
    }
  } while ((del = *delpo++) != 0);
  /* indtil ikke flere retninger */

  bd->ndiscs[opp] -= vendt;
  /* modstanderen fik vendt "vendt" brikker */

  vendt++;
  bd->ndiscs[pl] += vendt;
  /* spilleren fik "vendt" + 1 brik, da han ogsaa satte en brik */
  /* paa breaddet                                               */
}

/*******************************************************************/
/* trylist er en funktion, der kun bruges af den hurtige slutspils */
/* algoritme. Der er ikke noget specialt ved den, det er blot      */
/* trymove, og makelist der er blevet slaaet sammen i en funktion  */
/* saa der spares et funktionskald, og noget opsaetning, selve     */
/* koden er taget direkte fra de to funktioner, saa forklaringen   */
/* kan laeses der                                                  */
/*******************************************************************/

void try1list(movelist *legal,short int trysq, short int pl, board *bd)
{
  short int i,k1,opp,del,*delpo,trek,vendt;
  char *mpoi,*mslut,*mvpo,*bradt;

  mpoi = bd->possible.move;
  mslut = (mpoi + --bd->possible.nmoves);

  while (*mpoi++ != (char)trysq);
  *(mpoi - 1) = *mslut;

  opp = other(pl);
  bd->sq[trysq] = (char)pl;
  delpo = dirs1[trysq];
  del = *delpo++;
  vendt = 0;
  do {
    k1 = trysq + del;
    if (bd->sq[k1] == (char)opp) {
      do k1 += del;
      while (bd->sq[k1] == (char)opp);
      if (bd->sq[k1] == (char)pl) {
        k1 -= del;
        do {
          bd->sq[k1] = (char)pl;
          vendt++;
          k1 -= del;
        }
        while (k1 != trysq);
      }
    }
    else if (bd->sq[k1] != (char)pl) {
      mpoi = bd->possible.move;
      *mslut = (char)k1;
      while (*mpoi++ != (char)k1);
      if (mpoi > mslut) {
        bd->possible.nmoves++;
        mslut++;
      }
    }
  } while ((del = *delpo++) != 0);
  bd->ndiscs[opp] -= vendt;
  vendt++;
  bd->ndiscs[pl] += vendt;

  legal->nmoves = 0;
  mvpo = legal->move;
  i = bd->possible.nmoves - 1;
  /* her er trylist, forskellig fra makelist, da vi ved at trylist */
  /* aldrig bruges ved en slutknude, saa ved vi at i er stoerre    */
  /* eller lig 0 forste gang, og kan derfor vente med at checke    */
  /* dette til sidst i loekken                                     */
  do {
    delpo = dirs[trek = bd->possible.move[i]];
    del = *delpo++;
    do {
      bradt = (char *)(&bd->sq[trek]) + del;
      if (*bradt == (char)pl) {
        do bradt += del;
        while (*bradt == (char)pl);
        if (*bradt == (char)opp) {
          *mvpo++ = (char)trek;
          legal->nmoves++;
          break;
        }
      }
    } while ((del = *delpo++) != 0);
  } while (--i >= 0);
} 


void trylist(movelist *legal,short int trysq, short int pl, board *bd)
{
  short int i,k1,opp,del,*delpo,trek,vendt;
  char *mpoi,*mslut,*mvpo,*bradt;

  mpoi = bd->possible.move;
  mslut = (mpoi + --bd->possible.nmoves);

  while (*mpoi++ != (char)trysq);
  *(mpoi - 1) = *mslut;

  opp = other(pl);
  bd->sq[trysq] = (char)pl;
  delpo = dirs1[trysq];
  del = *delpo++;
  vendt = 0;
  do {
    k1 = trysq + del;
    if (bd->sq[k1] == (char)opp) {
      do k1 += del;
      while (bd->sq[k1] == (char)opp);
      if (bd->sq[k1] == (char)pl) {
        k1 -= del;
        do {
          bd->sq[k1] = (char)pl;
          vendt++;
          k1 -= del;
        }
        while (k1 != trysq);
      }
    }
  } while ((del = *delpo++) != 0);
  bd->ndiscs[opp] -= vendt;
  vendt++;
  bd->ndiscs[pl] += vendt;

  legal->nmoves = 0;
  mvpo = legal->move;
  i = bd->possible.nmoves - 1;
  /* her er trylist, forskellig fra makelist, da vi ved at trylist */
  /* aldrig bruges ved en slutknude, saa ved vi at i er stoerre    */
  /* eller lig 0 forste gang, og kan derfor vente med at checke    */
  /* dette til sidst i loekken                                     */
  do {
    delpo = dirs[trek = bd->possible.move[i]];
    del = *delpo++;
    do {
      bradt = (char *)(&bd->sq[trek]) + del;
      if (*bradt == (char)pl) {
        do bradt += del;
        while (*bradt == (char)pl);
        if (*bradt == (char)opp) {
          *mvpo++ = (char)trek;
          legal->nmoves++;
          break;
        }
      }
    } while ((del = *delpo++) != 0);
  } while (--i >= 0);
}

#endif /* FAST */

movelist tagne;

void makemove(short int k, short int pl)
{       /* Make move on main board */  
  short int dir,k1,opponent,del,x;

  mainboard.sq[k] = (char)pl;
  opponent = other(pl);
  x = 0;
  mainboard.ndiscs[pl] ++;
  delmove(k, &mainboard.possible);
  for (dir = NORTH; dir <= NORTHWEST; dir++) {
    del = delta[dir];
    if (flanking(k, dir, &mainboard, pl)) {
      k1 = k + del;
      do {
        tagne.move[x++] = (char)k1;
        mainboard.sq[k1] = (char)pl;
        mainboard.ndiscs[pl]++;
        mainboard.ndiscs[opponent]--;
        k1 += del;
      } while (mainboard.sq[k1] != pl);
    }
    else if (mainboard.sq[k + del] == EMPTY) 
      addmove(k + del, &mainboard.possible);
  }
  tagne.nmoves = (char)x;
  game.moves[playnm++] = (char)k;
  game.sidste = playnm;
  game.boards[playnm] = mainboard;
//  update_move(playnm);
}

/**************************************************************/
/* Her initialiseres de globale variable, det drejer sig om   */
/* arrayet delta, der indeholder de vaerdier der skal laegges */
/* til et felt for at bevaege sig i de forskellige retninger  */
/**************************************************************/

void init_rev(void)
{
  delta[NORTH] = -10;
  delta[NORTHEAST] = -9;
  delta[EAST] = 1;
  delta[SOUTHEAST] = 11;
  delta[SOUTH] = 10;
  delta[SOUTHWEST] = 9;
  delta[WEST] = -1;
  delta[NORTHWEST] = -11;
}

/*****************************************************************/
/* init_game opsaetter de forskellige variable som de skal vaere */
/* ved starten af spillet                                        */
/*****************************************************************/

void init_game(void)
{
  short int i,j;

  /* foerst saetter vi greansefelterne */
  for (i = 0; i <= 9; i++) {
    mainboard.sq[i] = BORDER;
    mainboard.sq[i + 90] = BORDER;
    mainboard.sq[10*i] = BORDER;
    mainboard.sq[10*i +9] = BORDER;
   }

  /* hver spiller har 2 brikker */
  mainboard.ndiscs[LIGHT] = 2;
  mainboard.ndiscs[DARK] = 2;

  /* Listen af mulige treak oprettes */
  mainboard.possible.nmoves = 12;
  mainboard.possible.move[0] = 33;
  mainboard.possible.move[1] = 34;
  mainboard.possible.move[2] = 35;
  mainboard.possible.move[3] = 36;
  mainboard.possible.move[4] = 43;
  mainboard.possible.move[5] = 46;
  mainboard.possible.move[6] = 53;
  mainboard.possible.move[7] = 56;
  mainboard.possible.move[8] = 63;
  mainboard.possible.move[9] = 64;
  mainboard.possible.move[10] = 65;
  mainboard.possible.move[11] = 66;

  /* breadtfelterne saettes til at vaere tomme */
  for (i = 1; i <= 8; i++)
    for (j = 1; j <= 8; j++)
       mainboard.sq[10*i + j] = EMPTY;

  /* de fire startbrikker saettes pa breaddet */
  mainboard.sq[44] = LIGHT;
  mainboard.sq[55] = LIGHT;
  mainboard.sq[45] = DARK;
  mainboard.sq[54] = DARK;

  /* spilleren er sort (Der kan selvfoegelig skiftes side) */
  human = DARK;
  computer = LIGHT;
  /* logikvariable initialiseres */
  gameover = calc = b1calc = bcalc = FALSE;
  /* liste af mulige treak genereres, bruges af short interfacet til */
  /* visning af de mulige treak                                */
  makelist(&list, human, &mainboard);

  /* startposition gemmes i vores liste af stillinger, bruges   */
  /* naar vi gemmer et spil, og naar vi bladrer frem og tilbage */
  /* i et spil                                                  */
  playnm = 0;
  game.sidste = playnm;
  game.moves[playnm] = 0;
  game.boards[playnm] = mainboard;

  /* timeleft saettes til startvaerdien for de nuvaerende spil- */
  /* niveau                                                     */
	if (tid_kontrol == spil_tid) {
		timeleft[0] = GameTid*60*1000;
		timeleft[1] = GameTid*60*1000;
	}
	else {
		timeleft[0] = spil_tider[lookahead];
		timeleft[1] = spil_tider[lookahead];
	}
}
