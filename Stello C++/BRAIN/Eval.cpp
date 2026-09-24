/****************************************************************************/
/*                                                                          */
/*                  eval.c ,    evalueringsfunktionen                       */
/*                                                                          */
/****************************************************************************/
#include "stdafx.h"
#include "reversi.h"

#define KANTER 6561     /* 3 i 8 forskellige kanter */

extern short int hjo_trek[KANTER],sikker[KANTER], black_v[KANTER], 
black_h[KANTER], white_v[KANTER], white_h[KANTER], white_m[KANTER], black_m[KANTER];


extern short int sindex1,sindex2,sindex3,sindex4;
extern long HashCall, HashHit; 

/* makroer til at bestemme om hjoernetraek er mulige  */
#define MIDT_HUMAN 0x2000
#define MIDT_COMPUTER 0x1000
#define HVID_STABIL_HJ1 0x800
#define HVID_STABIL_HJ2 0x400
#define SORT_HJ1 0x200
#define SORT_HJ2 0x100
#define FARLIG_HJ1_SORT 0x80
#define FARLIG_HJ2_SORT 0x40
#define FARLIG_HJ1_HVID 0x20
#define FARLIG_HJ2_HVID 0x10
#define SORT_STABIL_HJ1 0x08
#define SORT_STABIL_HJ2 0x04
#define HVID_HJ1 0x02
#define HVID_HJ2 0x01

#define BONUS  400
#define CURMOB 400l
#define POTMOB 600l

#define min(a, b)       ((a) < (b) ? (a) : (b))
#define max(a, b)       ((a) > (b) ? (a) : (b))

/* her vises hvordan en stilling skal forstaas */
/*  h1 er hjoerne 1                            */

/**********************/
/*   h1  side 1  h2   */
/*     **********     */
/*     *00000000*     */
/*   s *00000000* s   */
/*   i *00000000* i   */
/*   d *00000000* d   */
/*   e *00000000* e   */
/*     *00000000*     */
/*   4 *00000000* 2  */
/*     *00000000*     */
/*     **********     */
/*   h4  side 3  h3   */
/**********************/

/*************************************************/
/* eval er den funktion der evaluerer stillinger */
/*************************************************/

short int eval(short int alfa,
         short int beta,
         short int plmov,
         short int player,
         board *bd)
{
  short int index1,index2,index3,index4,hj1,hj2,hj3,hj4,score
						,tindex1,tindex2,tindex3,tindex4,tscore,fscore
						,midt1,midt2,midt3,midt4,smidt1,smidt2,smidt3,smidt4
            ,phj1,phj2,phj3,phj4,opp
            ,sj1,sj2,sj3,sj4,fsj1,fsj2,fsj3,fsj4
            ,psj1,psj2,psj3,psj4
            ,fhj1,fhj2,fhj3,fhj4,flatscore,ndisc,pbonus
            ,xs,ys,oxs,oys,start,pl,felt,stabel,humdis,comdis,dissco,
						sco_sgn, * com_v, * com_h, *hum_v, *hum_h, * com_m, * hum_m;

  short int potential[2];
  int commov,hummov;
  long escore,propc;

  aeval++;
  aknud++;
	opp = other(player);

  /* her undersoges om spillet er forbi, dette gaelder */
  /* hvis en af spillerne ikke har nogle brikker       */

  ndisc = bd->ndiscs[player] + bd->ndiscs[opp];

  if (bd->ndiscs[player] == 0)
    return -32600 - bd->ndiscs[opp];
  else if (bd->ndiscs[opp] == 0)
    return 32600 + bd->ndiscs[player];

  if (alfa > 32600)
    /* hvis vi har fundet en treakreakkefolge der foerer til afslutningen */
    /* af spillet, saa er der ingen grund til at udfoere de resterende    */
    /* beregninger, vi retunerer blot -32000 for at vise at denne         */
    /* stilling er daarligere                                             */
    return -32600;

  /* vi beregner index til opslag i vores tabeller  */
  /* da tabellen er beregnet for hvid i treakket    */
  /* beregnes indexet paa to maader, hvis spilleren */
  /* er sort, da beregnes indexet normalt, hvor     */
  /* hvert felt har 3 mulige vaerdier, 0 ,1 og 2    */
  /* hvor hvid er 0, sort er 1 og tom er 2          */
  /* hvis spilleren er hvid, da er det sort som     */
  /* treakke, og vi slaar op i tabellen med hvid    */
  /* 1, sort som 0 og tom som 2,                    */

  index1 = sindex1;
  index2 = sindex2;
  index3 = sindex3;
  index4 = sindex4;

  if (player == DARK) {
    /* da tabelvaerdierne er for hvid, skal de inverteres */
    /* hvis computeren er sort, og det modsatte skal ske  */
    /* naar vi finder vaerdierne for den inverse stilling */
		fhj1 = (hjo_trek[index1] & FARLIG_HJ1_SORT) | (hjo_trek[index4]  & FARLIG_HJ1_SORT);
		hj1 = (hjo_trek[index1] & SORT_HJ1) | (hjo_trek[index4]  & SORT_HJ1);
		fhj2 = (hjo_trek[index1] & FARLIG_HJ2_SORT) | (hjo_trek[index2]  & FARLIG_HJ1_SORT);
		hj2 = (hjo_trek[index1] & SORT_HJ2) | (hjo_trek[index2]  & SORT_HJ1);
		fhj3 = (hjo_trek[index2] & FARLIG_HJ2_SORT) | (hjo_trek[index3]  & FARLIG_HJ2_SORT);
		hj3 = (hjo_trek[index2] & SORT_HJ2) | (hjo_trek[index3]  & SORT_HJ2);
		fhj4 = (hjo_trek[index4] & FARLIG_HJ2_SORT) | (hjo_trek[index3]  & FARLIG_HJ1_SORT);
		hj4 = (hjo_trek[index4] & SORT_HJ2) | (hjo_trek[index3]  & SORT_HJ1);
		
		fsj1 = (hjo_trek[index1] & FARLIG_HJ1_HVID) | (hjo_trek[index4]  & FARLIG_HJ1_HVID);
		sj1 = (hjo_trek[index1] & HVID_STABIL_HJ1) | (hjo_trek[index4]  & HVID_STABIL_HJ1);
		fsj2 = (hjo_trek[index1] & FARLIG_HJ2_HVID) | (hjo_trek[index2]  & FARLIG_HJ1_HVID);
		sj2 = (hjo_trek[index1] & HVID_STABIL_HJ2) | (hjo_trek[index2]  & HVID_STABIL_HJ1);
		fsj3 = (hjo_trek[index2] & FARLIG_HJ2_HVID) | (hjo_trek[index3]  & FARLIG_HJ2_HVID);
		sj3 = (hjo_trek[index2] & HVID_STABIL_HJ2) | (hjo_trek[index3]  & HVID_STABIL_HJ2);
		fsj4 = (hjo_trek[index4] & FARLIG_HJ2_HVID) | (hjo_trek[index3]  & FARLIG_HJ1_HVID);
		sj4 = (hjo_trek[index4] & HVID_STABIL_HJ2) | (hjo_trek[index3]  & HVID_STABIL_HJ1);
		midt1 = hjo_trek[index1] & MIDT_HUMAN;
		midt2 = hjo_trek[index2] & MIDT_HUMAN;
		midt3 = hjo_trek[index3] & MIDT_HUMAN;
		midt4 = hjo_trek[index4] & MIDT_HUMAN;
		smidt1 = hjo_trek[index1] & MIDT_COMPUTER;
		smidt2 = hjo_trek[index2] & MIDT_COMPUTER;
		smidt3 = hjo_trek[index3] & MIDT_COMPUTER;
		smidt4 = hjo_trek[index4] & MIDT_COMPUTER;

	}
	else {
		fhj1 = (hjo_trek[index1] & FARLIG_HJ1_HVID) | (hjo_trek[index4]  & FARLIG_HJ1_HVID);
		hj1 = (hjo_trek[index1] & HVID_HJ1) | (hjo_trek[index4]  & HVID_HJ1);
		fhj2 = (hjo_trek[index1] & FARLIG_HJ2_HVID) | (hjo_trek[index2]  & FARLIG_HJ1_HVID);
		hj2 = (hjo_trek[index1] & HVID_HJ2) | (hjo_trek[index2]  & HVID_HJ1);
		fhj3 = (hjo_trek[index2] & FARLIG_HJ2_HVID) | (hjo_trek[index3]  & FARLIG_HJ2_HVID);
		hj3 = (hjo_trek[index2] & HVID_HJ2) | (hjo_trek[index3]  & HVID_HJ2);
		fhj4 = (hjo_trek[index4] & FARLIG_HJ2_HVID) | (hjo_trek[index3]  & FARLIG_HJ1_HVID);
		hj4 = (hjo_trek[index4] & HVID_HJ2) | (hjo_trek[index3]  & HVID_HJ1);
		fsj1 = (hjo_trek[index1] & FARLIG_HJ1_SORT) | (hjo_trek[index4]  & FARLIG_HJ1_SORT);
		sj1 = (hjo_trek[index1] & SORT_STABIL_HJ1) | (hjo_trek[index4]  & SORT_STABIL_HJ1);
		fsj2 = (hjo_trek[index1] & FARLIG_HJ2_SORT) | (hjo_trek[index2]  & FARLIG_HJ1_SORT);
		sj2 = (hjo_trek[index1] & SORT_STABIL_HJ2) | (hjo_trek[index2]  & SORT_STABIL_HJ1);
		fsj3 = (hjo_trek[index2] & FARLIG_HJ2_SORT) | (hjo_trek[index3]  & FARLIG_HJ2_SORT);
		sj3 = (hjo_trek[index2] & SORT_STABIL_HJ2) | (hjo_trek[index3]  & SORT_STABIL_HJ2);
		fsj4 = (hjo_trek[index4] & FARLIG_HJ2_SORT) | (hjo_trek[index3]  & FARLIG_HJ1_SORT);
		sj4 = (hjo_trek[index4] & SORT_STABIL_HJ2) | (hjo_trek[index3]  & SORT_STABIL_HJ1);
		midt1 = hjo_trek[index1] & MIDT_COMPUTER;
		midt2 = hjo_trek[index2] & MIDT_COMPUTER;
		midt3 = hjo_trek[index3] & MIDT_COMPUTER;
		midt4 = hjo_trek[index4] & MIDT_COMPUTER;
		smidt1 = hjo_trek[index1] & MIDT_HUMAN;
		smidt2 = hjo_trek[index2] & MIDT_HUMAN;
		smidt3 = hjo_trek[index3] & MIDT_HUMAN;
		smidt4 = hjo_trek[index4] & MIDT_HUMAN;

	}
		
   if (player == DARK) {
			com_v = black_v;
			com_h = black_h;
			com_m = black_m;
			hum_v = white_v;
			hum_h = white_h;
			hum_m = white_m;
			sco_sgn = -1;

    } 
		else {
			com_v = white_v;
			com_h = white_h;
			com_m = white_m;
			hum_v = black_v;
			hum_h = black_h;
			hum_m = black_m;
			sco_sgn = 1;
    }

#ifndef NOEXTEN

  /* fhj1 er en boolsk variabel, der ved tabelopslag afgoer om */
  /* hjoerne 1 er et tilladt hjoerne treak, da vores algoritme */
  /* ikke skal give forkerte vaerdier for bestemte stillinger  */
  /* er disse paa forhand katagoriseret som urolige, og vil    */
  /* ikke blive evaluret i slutknuderne, for at faa en nojag-  */
  /* tig evaluering, skal det treak der foerte frem til denne  */
  /* stilling ske foer selve slutknuden. Dette sikrer, at det  */
  /* er mulig for programmet at afgoere om der findes et godt  */
  /* modtreak til dette treak. Altsaa at stillingen foerst     */
  /* evalueres naar den er forholdsvis rolig                   */

/*  fhj1 = (hjo_trek[index1] & FARLIG_HJ1_HVID) | (hjo_trek[index4]  & FARLIG_HJ1_HVID); */

  /* phj1 er en boolsk variabel, der bruges hvis spilleren     */
  /* har en brik paa et x-felt og hjoernet er tomt, og         */
  /* modstanderen ikke i naeste treak kan vende x-feltet og    */
  /* dermed naa hjoernet. Hvis dette er tilfaeldet, skal det   */
  /* give en negativ vaerdi, da modstanderen sandesyneligvis   */
  /* paa et eller andet tidspunkt kan naa hjoernet gennem      */
  /* dette x-felt.                                             */

  phj1 = FALSE;
  if (fhj1)
    hj1 = FALSE;
  else {
    /* hj1 er en boolsk variabel, der afgoer om spilleren kan   */
    /* foretage sig et hjoerne treak. Dette afgoeres ved foerst */
    /* at laeve et tabelopslag, for at se om det kan ska fra en */
    /* kant, og hvis dette ikke er tilfaeldet, ses det om       */
    /* hjoernet kan naas gennem et x-felt.                      */

/*    hj1 = (hjo_trek[index1] & HVID_HJ1) | (hjo_trek[index4]  & HVID_HJ1); */
    if (!hj1 && (bd->sq[11] == EMPTY) && (bd->sq[22] == (char)opp)) {
      phj1 = TRUE;
      if (bd->sq[33] == (char)player)
        hj1 = TRUE;
      else if (bd->sq[33] == (char)opp)
        if (bd->sq[44] == (char)player)
          hj1 = TRUE;
        else if (bd->sq[44] == (char)opp)
          if (bd->sq[55] == (char)player)
            hj1 = TRUE;
          else if (bd->sq[55] == (char)opp)
            if (bd->sq[66] == (char)player)
              hj1 = TRUE;
            else if (bd->sq[66] == (char)opp)
              if (bd->sq[77] == (char)player)
                hj1 = TRUE;
              else if (bd->sq[77] == (char)opp)
                if (bd->sq[88] == (char)player)
                  hj1 = TRUE;
     }
  }

  /* som ovenstaaende for hjoerne 2 */

/*  fhj2 = (hjo_trek[index1] & FARLIG_HJ2_HVID) | (hjo_trek[index2]  & FARLIG_HJ1_HVID); */
  phj2 = FALSE;

  if (fhj2)
    hj2 = FALSE;
  else {
 /*   hj2 = (hjo_trek[index1] & HVID_HJ2) | (hjo_trek[index2]  & HVID_HJ1); */
    if (!hj2 && (bd->sq[18] == EMPTY) && (bd->sq[27] == (char)opp)) {
      phj2 = TRUE;
      if (bd->sq[36] == (char)player)
        hj2 = TRUE;
      else if (bd->sq[36] == (char)opp)
        if (bd->sq[45] == (char)player)
          hj2 = TRUE;
        else if (bd->sq[45] == (char)opp)
          if (bd->sq[54] == (char)player)
            hj2 = TRUE;
          else if (bd->sq[54] == (char)opp)
            if (bd->sq[63] == (char)player)
              hj2 = TRUE;
            else if (bd->sq[63] == (char)opp)
              if (bd->sq[72] == (char)player)
                hj2 = TRUE;
              else if (bd->sq[72] == (char)opp)
                if (bd->sq[81] == (char)player)
                  hj2 = TRUE;
     }
  }

  /* som ovenstaaende for hjoerne 3 */

/*  fhj3 = (hjo_trek[index2] & FARLIG_HJ2_HVID) | (hjo_trek[index3]  & FARLIG_HJ2_HVID); */
  phj3 = FALSE;

  if (fhj3)
    hj3 = FALSE;
  else {
/*    hj3 = (hjo_trek[index2] & HVID_HJ2) | (hjo_trek[index3]  & HVID_HJ2); */
    if (!hj3 && (bd->sq[88] == EMPTY) && (bd->sq[77] == (char)opp)) {
      phj3 = TRUE;
      if (bd->sq[66] == (char)player)
        hj3 = TRUE;
      else if (bd->sq[66] == (char)opp)
        if (bd->sq[55] == (char)player)
          hj3 = TRUE;
        else if (bd->sq[55] == (char)opp)
          if (bd->sq[44] == (char)player)
            hj3 = TRUE;
          else if (bd->sq[44] == (char)opp)
            if (bd->sq[33] == (char)player)
              hj3 = TRUE;
            else if (bd->sq[33] == (char)opp)
              if (bd->sq[22] == (char)player)
                hj3 = TRUE;
              else if (bd->sq[22] == (char)opp)
                if (bd->sq[11] == (char)player)
                  hj3 = TRUE;
     }
  }

  /* som ovenstaaende for hjoerne 4 */

/*  fhj4 = (hjo_trek[index4] & FARLIG_HJ2_HVID) | (hjo_trek[index3]  & FARLIG_HJ1_HVID); */
  phj4 = FALSE;

  if (fhj4)
    hj4 = FALSE;
  else {
/*    hj4 = (hjo_trek[index4] & HVID_HJ2) | (hjo_trek[index3]  & HVID_HJ1); */
    if (!hj4 && (bd->sq[81] == EMPTY) && (bd->sq[72] == (char)opp)) {
      phj4 = TRUE;
      if (bd->sq[63] == (char)player)
        hj4 = TRUE;
      else if (bd->sq[63] == (char)opp)
        if (bd->sq[54] == (char)player)
          hj4 = TRUE;
        else if (bd->sq[54] == (char)opp)
          if (bd->sq[45] == (char)player)
            hj4 = TRUE;
          else if (bd->sq[45] == (char)opp)
            if (bd->sq[36] == (char)player)
              hj4 = TRUE;
            else if (bd->sq[36] == (char)opp)
              if (bd->sq[27] == (char)player)
                hj4 = TRUE;
              else if (bd->sq[27] == (char)opp)
                if (bd->sq[18] == (char)player)
                  hj4 = TRUE;
     }
  }

  /* som ovenstaaende blot for modstanderen for hjoerne 1 */

/*  fsj1 = (hjo_trek[index1] & FARLIG_HJ1_SORT) | (hjo_trek[index4]  & FARLIG_HJ1_SORT); */
  psj1 = FALSE;
  if (fsj1)
    sj1 = FALSE;
  else {
 /*   sj1 = (hjo_trek[index1] & SORT_STABIL_HJ1) | (hjo_trek[index4]  & SORT_STABIL_HJ1); */
    if (!sj1 && (bd->sq[11] == EMPTY) && (bd->sq[22] == (char)player)) {
      psj1 = TRUE;
      if (bd->sq[33] == (char)opp)
        sj1 = TRUE;
      else if (bd->sq[33] == (char)player)
        if (bd->sq[44] == (char)opp)
          sj1 = TRUE;
        else if (bd->sq[44] == (char)player)
          if (bd->sq[55] == (char)opp)
            sj1 = TRUE;
          else if (bd->sq[55] == (char)player)
            if (bd->sq[66] == (char)opp)
              sj1 = TRUE;
            else if (bd->sq[66] == (char)player)
              if (bd->sq[77] == (char)opp)
                sj1 = TRUE;
              else if (bd->sq[77] == (char)player)
                if (bd->sq[88] == (char)opp)
                  sj1 = TRUE;
    }
  }

  /* som ovenstaaende for hjoerne 2 */

/*  fsj2 = (hjo_trek[index1] & FARLIG_HJ2_SORT) | (hjo_trek[index2]  & FARLIG_HJ1_SORT); */
  psj2 = FALSE;

  if (fsj2)
    sj2 = FALSE;
  else {
/*    sj2 = (hjo_trek[index1] & SORT_STABIL_HJ2) | (hjo_trek[index2]  & SORT_STABIL_HJ1); */
    if (!sj2 && (bd->sq[18] == EMPTY) && (bd->sq[27] == (char)player)) {
      psj2 = TRUE;
      if (bd->sq[36] == (char)opp)
        sj2 = TRUE;
      else if (bd->sq[36] == (char)player)
        if (bd->sq[45] == (char)opp)
          sj2 = TRUE;
        else if (bd->sq[45] == (char)player)
          if (bd->sq[54] == (char)opp)
            sj2 = TRUE;
          else if (bd->sq[54] == (char)player)
            if (bd->sq[63] == (char)opp)
              sj2 = TRUE;
            else if (bd->sq[63] == (char)player)
              if (bd->sq[72] == (char)opp)
                sj2 = TRUE;
              else if (bd->sq[72] == (char)player)
                if (bd->sq[81] == (char)opp)
                  sj2 = TRUE;
    }
  }

  /* som ovenstaaende for hjoerne 3 */

/*  fsj3 = (hjo_trek[index2] & FARLIG_HJ2_SORT) | (hjo_trek[index3]  & FARLIG_HJ2_SORT); */
  psj3 = FALSE;

  if (fsj3)
    sj3 = FALSE;
  else {
/*    sj3 = (hjo_trek[index2] & SORT_STABIL_HJ2) | (hjo_trek[index3]  & SORT_STABIL_HJ2); */
    if (!sj3 && (bd->sq[88] == EMPTY) && (bd->sq[77] == (char)player)) {
      psj3 = TRUE;
      if (bd->sq[66] == (char)opp)
        sj3 = TRUE;
      else if (bd->sq[66] == (char)player)
        if (bd->sq[55] == (char)opp)
          sj3 = TRUE;
        else if (bd->sq[55] == (char)player)
          if (bd->sq[44] == (char)opp)
            sj3 = TRUE;
          else if (bd->sq[44] == (char)player)
            if (bd->sq[33] == (char)opp)
              sj3 = TRUE;
            else if (bd->sq[33] == (char)player)
              if (bd->sq[22] == (char)opp)
                sj3 = TRUE;
              else if (bd->sq[22] == (char)player)
                if (bd->sq[11] == (char)opp)
                  sj3 = TRUE;
    }
  }

  /* som ovenstaaende for hjoerne 4 */

/*  fsj4 = (hjo_trek[index4] & FARLIG_HJ2_SORT) | (hjo_trek[index3]  & FARLIG_HJ1_SORT); */
  psj4 = FALSE;
  if (fsj4)
    sj4 = FALSE;
  else {
/*    sj4 = (hjo_trek[index4] & SORT_STABIL_HJ2) | (hjo_trek[index3]  & SORT_STABIL_HJ1); */
    if (!sj4 && (bd->sq[81] == EMPTY) && (bd->sq[72] == (char)player)) {
      psj4 = TRUE;
      if (bd->sq[63] == (char)opp)
        sj4 = TRUE;
      else if (bd->sq[63] == (char)player)
        if (bd->sq[54] == (char)opp)
          sj4 = TRUE;
        else if (bd->sq[54] == (char)player)
          if (bd->sq[45] == (char)opp)
            sj4 = TRUE;
          else if (bd->sq[45] == (char)player)
            if (bd->sq[36] == (char)opp)
              sj4 = TRUE;
            else if (bd->sq[36] == (char)player)
              if (bd->sq[27] == (char)opp)
                sj4 = TRUE;
              else if (bd->sq[27] == (char)player)
                if (bd->sq[18] == (char)opp)
                  sj4 = TRUE;
    }
  }



  /* propc bruges til at beregne hvor sandsyneligt det er */
  /* at en spiller kan vende en brik paa et x-felt op til */
  /* et tomt hjoerne, paa et eller andet tidspunkt        */

  propc = (int)(((long)(64l - ndisc)*1000l / 64l)/2l + 500l);

  pbonus = (int)(propc*BONUS/1000l);

  /* her udfores en selektiv alfa-beta sogning af dybde 2 */
  /* ud fra vaerdierne i vores tabeller                   */


/* spilleren i slutknuden er modstanderen */
           /* saa computeren er i treakket. dvs. at  */
           /* der skal maximeres, ellers foregaar    */
           /* alt som ovenfor                        */

    score = sco_sgn*(sikker[index1] + sikker[index2] + sikker[index3] + sikker[index4]);
    flatscore = score;
		
		if (smidt1)
			score = min(score,sco_sgn*(sikker[hum_m[index1]] + sikker[index2] + sikker[index3] + sikker[index4]));
		if (smidt2)
			score = min(score,sco_sgn*(sikker[index1] + sikker[hum_m[index2]] + sikker[index3] + sikker[index4]));
		if (smidt3)
			score = min(score,sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_m[index3]] + sikker[index4]));
		if (smidt4)
			score = min(score,sco_sgn*(sikker[index1] + sikker[index2] + sikker[index3] + sikker[hum_m[index4]]));

		
		if (sj1)
		  score = min(score,sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[index3] + sikker[hum_v[index4]]));
		else if (psj1 && !hj1) {
			escore = propc * (sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[index3] + sikker[hum_v[index4]])) +
               (1000 - propc)*flatscore;
      escore /= 1000;
			tscore = (short int)escore;

		  score = min(score, tscore);
		}

		if (sj2)
		  score = min(score,sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[index4]));
		else if (psj2 && !hj2) {
			escore = propc * (sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[index4])) +
               (1000 - propc)*flatscore;
      escore /= 1000;
			tscore = (short int)escore;

		  score = min(score, tscore);
		}

		if (sj3)
		  score = min(score,sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[index4]));
		else if (psj3 && !hj3) {
			escore = propc * (sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[index4])) +
               (1000 - propc)*flatscore;
      escore /= 1000;
			tscore = (short int)escore;

		  score = min(score, tscore);
		}

		if (sj4)
		  score = min(score,sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[index4]]));
		else if (psj4 && !hj4) {
			escore = propc * (sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[index4]])) +
               (1000 - propc)*flatscore;
      escore /= 1000;
			tscore = (short int)escore;

		  score = min(score, tscore);
		}


			
		if (midt1) {
      /* her kan spilleren nu foretage et hjoernetreak, og det   */
      /* udelukker, at modstanderen kan foretage det samme       */
      /* hjoernetreak.                                           */
			
			tindex1 = com_m[index1];
			fscore = tscore = sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[index3] + sikker[index4]);

				
			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[tindex1]] + sikker[index2] + sikker[index3] + sikker[index4])); 
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_m[index2]] + sikker[index3] + sikker[index4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_m[index3]] + sikker[index4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[index3] + sikker[hum_m[index4]]));

			
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[tindex1]] + sikker[index2] + sikker[index3] + sikker[hum_v[index4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[tindex1]] + sikker[index2] + sikker[index3] + sikker[hum_v[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj2)
				tscore = min(tscore,sco_sgn*(sikker[hum_h[tindex1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[index4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[tindex1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[index4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[index4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			score = max(tscore,score);		
    }


		if (midt2) {
      /* her kan spilleren nu foretage et hjoernetreak, og det   */
      /* udelukker, at modstanderen kan foretage det samme       */
      /* hjoernetreak.                                           */
			
			tindex2 = com_m[index2];
			fscore = tscore = sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[index3] + sikker[index4]);
			
			
			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[index1]] + sikker[tindex2] + sikker[index3] + sikker[index4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_m[tindex2]] + sikker[index3] + sikker[index4])); 
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_m[index3]] + sikker[index4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[index3] + sikker[hum_m[index4]]));

			
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[index1]] + sikker[tindex2] + sikker[index3] + sikker[hum_v[index4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[index1]] + sikker[tindex2] + sikker[index3] + sikker[hum_v[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj2)
				tscore = min(tscore,sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[tindex2]] + sikker[index3] + sikker[index4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[tindex2]] + sikker[index3] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_h[tindex2]] + sikker[hum_h[index3]] + sikker[index4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[hum_h[tindex2]] + sikker[hum_h[index3]] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_v[index3]] + sikker[hum_h[index4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_v[index3]] + sikker[hum_h[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}
				
			score = max(tscore,score);		
    }
		
		if (midt3) {
      /* her kan spilleren nu foretage et hjoernetreak, og det   */
      /* udelukker, at modstanderen kan foretage det samme       */
      /* hjoernetreak.                                           */
			
			tindex3 = com_m[index3];
			fscore = tscore = sco_sgn*(sikker[index1] + sikker[index2] + sikker[tindex3] + sikker[index4]);
			
			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[index1]] + sikker[index2] + sikker[tindex3] + sikker[index4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_m[index2]] + sikker[tindex3] + sikker[index4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_m[tindex3]] + sikker[index4])); 
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[tindex3] + sikker[hum_m[index4]]));

			
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[tindex3] + sikker[hum_v[index4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[tindex3] + sikker[hum_v[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj2)
				tscore = min(tscore,sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[tindex3] + sikker[index4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[tindex3] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[tindex3]] + sikker[index4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[tindex3]] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_v[tindex3]] + sikker[hum_h[index4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_v[tindex3]] + sikker[hum_h[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			score = max(tscore,score);		
    }
		
		if (midt4) {
      /* her kan spilleren nu foretage et hjoernetreak, og det   */
      /* udelukker, at modstanderen kan foretage det samme       */
      /* hjoernetreak.                                           */
			
			tindex4 = com_m[index4];
			fscore = tscore = sco_sgn*(sikker[index1] + sikker[index2] + sikker[index3] + sikker[tindex4]);
			
			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[index1]] + sikker[index2] + sikker[index3] + sikker[tindex4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_m[index2]] + sikker[index3] + sikker[tindex4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_m[index3]] + sikker[tindex4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[index3] + sikker[hum_m[tindex4]])); 
		
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[index3] + sikker[hum_v[tindex4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[index3] + sikker[hum_v[tindex4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj2)
				tscore = min(tscore,sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[tindex4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[tindex4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[tindex4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[tindex4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}
				
			score = max(tscore,score);	
    }


    if (hj1) {
			tindex1 = com_v[index1];
			tindex4 = com_v[index4];
			fscore = tscore = sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[index3] + sikker[tindex4]);

			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[tindex1]] + sikker[index2] + sikker[index3] + sikker[tindex4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_m[index2]] + sikker[index3] + sikker[tindex4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_m[index3]] + sikker[tindex4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[index3] + sikker[hum_m[tindex4]]));
			
			if (sj2)
				tscore = min(tscore,sco_sgn*(sikker[hum_h[tindex1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[tindex4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[tindex1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[tindex4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[tindex4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[tindex4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}
				
			score = max(tscore,score);
    }
    else if (phj1 && !sj1) {
			tindex1 = com_v[index1];
			tindex4 = com_v[index4];
		  escore = propc * (sco_sgn*(sikker[com_v[index1]] + sikker[index2] + sikker[index3] + sikker[com_v[index4]])) + 
							(1000 - propc)*flatscore;
      escore /= 1000;		
			fscore = tscore = (short int)escore;
			
			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[tindex1]] + sikker[index2] + sikker[index3] + sikker[tindex4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_m[index2]] + sikker[index3] + sikker[tindex4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_m[index3]] + sikker[tindex4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[index3] + sikker[hum_m[tindex4]]));
			
			if (sj2)
				tscore = min(tscore,sco_sgn*(sikker[hum_h[tindex1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[tindex4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[tindex1]] + sikker[hum_v[index2]] + sikker[index3] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[tindex4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[hum_h[index2]] + sikker[hum_h[index3]] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[tindex4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[index2] + sikker[hum_v[index3]] + sikker[hum_h[tindex4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}
				
			score = max(tscore,score);
    }

    if (hj2) {
			tindex1 = com_h[index1];
			tindex2 = com_v[index2];
			fscore = tscore = sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[index3] + sikker[index4]);

			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[tindex1]] + sikker[tindex2] + sikker[index3] + sikker[index4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_m[tindex2]] + sikker[index3] + sikker[index4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[hum_m[index3]] + sikker[index4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[index3] + sikker[hum_m[index4]]));
			
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[tindex1]] + sikker[tindex2] + sikker[index3] + sikker[hum_v[index4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[tindex1]] + sikker[tindex2] + sikker[index3] + sikker[hum_v[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_h[tindex2]] + sikker[hum_h[index3]] + sikker[index4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[hum_h[tindex2]] + sikker[hum_h[index3]] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[hum_v[index3]] + sikker[hum_h[index4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[hum_v[index3]] + sikker[hum_h[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			score = max(tscore,score);
    }
    else if (phj2 && !sj2) {

			tindex1 = com_h[index1];
			tindex2 = com_v[index2];
			escore = propc * (sco_sgn*(sikker[com_h[index1]] + sikker[com_v[index2]] + sikker[index3] + sikker[index4]) ) +
			         (1000 - propc)*flatscore;
      escore /= 1000;
			fscore = tscore = (short int)escore;
			
			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[tindex1]] + sikker[tindex2] + sikker[index3] + sikker[index4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_m[tindex2]] + sikker[index3] + sikker[index4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[hum_m[index3]] + sikker[index4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[index3] + sikker[hum_m[index4]]));

			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[tindex1]] + sikker[tindex2] + sikker[index3] + sikker[hum_v[index4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[tindex1]] + sikker[tindex2] + sikker[index3] + sikker[hum_v[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = min(tscore, (short)escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[hum_h[tindex2]] + sikker[hum_h[index3]] + sikker[index4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[hum_h[tindex2]] + sikker[hum_h[index3]] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = min(tscore, (short)escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[hum_v[index3]] + sikker[hum_h[index4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[tindex1] + sikker[tindex2] + sikker[hum_v[index3]] + sikker[hum_h[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}
				
			score = max(tscore,score);
    }

    if (hj3) {
			tindex2 = com_h[index2];
			tindex3 = com_h[index3];
			fscore = tscore = sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[tindex3] + sikker[index4]);

			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[index1]] + sikker[tindex2] + sikker[tindex3] + sikker[index4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_m[tindex2]] + sikker[tindex3] + sikker[index4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_m[tindex3]] + sikker[index4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[tindex3] + sikker[hum_m[index4]]));
			
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[index1]] + sikker[tindex2] + sikker[tindex3] + sikker[hum_v[index4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[index1]] + sikker[tindex2] + sikker[tindex3] + sikker[hum_v[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj2)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[tindex2]] + sikker[tindex3] + sikker[index4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[tindex2]] + sikker[tindex3] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_v[tindex3]] + sikker[hum_h[index4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_v[tindex3]] + sikker[hum_h[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			score = max(tscore,score);
    }
    else if (phj3 && !sj3) {
			tindex2 = com_h[index2];
			tindex3 = com_h[index3];
			escore = propc * (sco_sgn*(sikker[index1] + sikker[com_h[index2]] + sikker[com_h[index3]] + sikker[index4])) + 
			         (1000 - propc)*flatscore;
      escore /= 1000;
			fscore = (short int)tscore = escore;

			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[index1]] + sikker[tindex2] + sikker[tindex3] + sikker[index4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_m[tindex2]] + sikker[tindex3] + sikker[index4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_m[tindex3]] + sikker[index4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[tindex3] + sikker[hum_m[index4]]));
			
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[index1]] + sikker[tindex2] + sikker[tindex3] + sikker[hum_v[index4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[index1]] + sikker[tindex2] + sikker[tindex3] + sikker[hum_v[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj2)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[tindex2]] + sikker[tindex3] + sikker[index4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[tindex2]] + sikker[tindex3] + sikker[index4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_v[tindex3]] + sikker[hum_h[index4]]));
			else if (psj4 && !hj4) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[tindex2] + sikker[hum_v[tindex3]] + sikker[hum_h[index4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			score = max(tscore,score);
    }

    if (hj4) {
			tindex3 = com_v[index3];
			tindex4 = com_h[index4];
			fscore = tscore = sco_sgn*(sikker[index1] + sikker[index2] + sikker[tindex3] + sikker[tindex4]);

			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[index1]] + sikker[index2] + sikker[tindex3] + sikker[tindex4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_m[index2]] + sikker[tindex3] + sikker[tindex4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_m[tindex3]] + sikker[tindex4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[tindex3] + sikker[hum_m[tindex4]]));
			
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[tindex3] + sikker[hum_v[tindex4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[tindex3] + sikker[hum_v[tindex4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj2)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[tindex3] + sikker[tindex4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[tindex3] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[tindex3]] + sikker[tindex4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[tindex3]] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}
				
			score = max(tscore,score);
    }
    else if (phj4 && !sj4) {
			tindex3 = com_v[index3];
			tindex4 = com_h[index4];
			escore = propc * (sco_sgn*(sikker[index1] + sikker[index2] + sikker[com_v[index3]] + sikker[com_h[index4]])) + 
			         (1000 - propc)*flatscore;
      escore /= 1000;	
			fscore = (short int)tscore = escore;

			if (smidt1)
				tscore = min(tscore,sco_sgn*(sikker[hum_m[index1]] + sikker[index2] + sikker[tindex3] + sikker[tindex4]));
			if (smidt2)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_m[index2]] + sikker[tindex3] + sikker[tindex4]));
			if (smidt3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[hum_m[tindex3]] + sikker[tindex4]));
			if (smidt4)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[index2] + sikker[tindex3] + sikker[hum_m[tindex4]]));
	
			if (sj1)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[tindex3] + sikker[hum_v[tindex4]]));
			else if (psj1 && !hj1) {
				escore = propc * (sco_sgn*(sikker[hum_v[index1]] + sikker[index2] + sikker[tindex3] + sikker[hum_v[tindex4]])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj2)
		  	tscore = min(tscore,sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[tindex3] + sikker[tindex4]));
			else if (psj2 && !hj2) {
				escore = propc * (sco_sgn*(sikker[hum_h[index1]] + sikker[hum_v[index2]] + sikker[tindex3] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			if (sj3)
				tscore = min(tscore,sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[tindex3]] + sikker[tindex4]));
			else if (psj3 && !hj3) {
				escore = propc * (sco_sgn*(sikker[index1] + sikker[hum_h[index2]] + sikker[hum_h[tindex3]] + sikker[tindex4])) +
									(1000 - propc)*fscore;
				escore /= 1000;
				score = (short int)min(tscore, escore);
			}

			score = max(tscore,score);
    }

#else

  /* her beregnes scoren uden selektiv uddybning */
  score = imov1 + imov2 + imov3 + imov4;

#endif


  /* folgende algoritme er til at beregne intern stabilitet  */
  /* foerst undersoges om et hjoerne er besat, og derefter   */
  /* taelles de stabile brikker ved at bevaege sig diagonalt */
  /* ud fra hjoernet og derefter taelle de stabile brikker   */
  /* henholdsvis lodret og vandret, hvis der er en stabil    */
  /* brik i den ene retning og mindst 2 i den anden retning  */
  /* saa er naeste brik i diagonal retning ogsaa stabil      */
  /* Denne metode finder ikke alle internt stabile brikker   */
  /* men den er hurtig og bedre end ingenting. Desuden saa   */
  /* har den ikke saa stor betydning i praksis, da naar der  */
  /* findes stabile interne brikke, som regel er muligt at   */
  /* loese spillet til enden                                 */

  if (bd->sq[11] == (char)player || bd->sq[11] == (char)opp) {
    pl = bd->sq[11];

    start = 11;

    felt = 12;
    xs = 0;
    while (bd->sq[felt] == (char)pl) {
      felt++;
      xs++;
    }
    felt = 21;
    ys = 0;
    while (bd->sq[felt] == (char)pl) {
      felt += 10;
      ys++;
    }

    stabel = 0;  /* de foerste raekker taelles ikke med da de */
                 /* er indeholdt i vores kanttabeller         */


    while ((xs >= 1 && ys > 1) || (xs > 1 && ys >= 1)) {
      /* saalaenge der er mindst 1 og mindst 2 stabiler */
      /* brikker i vandret og lodret retning saa gaa    */
      /* diagonalt                                      */
      start += 11;
      stabel ++;

      felt = start + 1;
      oxs = xs - 2;
      xs = 0;
      while (bd->sq[felt] == (char)pl && (oxs-- > 0)) {
        felt++;
        xs ++; /* tael de stabile brikker i vandret retning */
      }
      stabel += xs;
      felt = start + 10;
      oys = ys - 2;
      ys = 0;
      while (bd->sq[felt] == (char)pl && (oys-- > 0)) {
        felt += 10;
        ys++;  /* tael de stabile brikker i lodret retning */
      }
      stabel += ys;
    }
    if (pl == player)
      score += 60*stabel;
    else
      score -= 60*stabel;
  }

  /* som ovenfor udfra hjoerne 2 */

  if (bd->sq[18] == (char)player || bd->sq[18] == (char)opp) {
    pl = bd->sq[18];

    start = 18;

    felt = 17;
    xs = 0;
    while (bd->sq[felt] == (char)pl) {
      felt--;
      xs++;
    }
    felt = 28;
    ys = 0;
    while (bd->sq[felt] == (char)pl) {
      felt += 10;
      ys++;
    }
    stabel = 0;
    while ((xs >= 1 && ys > 1) || (xs > 1 && ys >= 1)) {
      start += 9;
      stabel ++;
      felt = start - 1;
      oxs = xs - 2;
      xs = 0;
      while (bd->sq[felt] == (char)pl && (oxs-- > 0)) {
        felt--;
        xs ++;
      }
      stabel += xs;
      felt = start + 10;
      oys = ys - 2;
      ys = 0;
      while (bd->sq[felt] == (char)pl && (oys-- > 0)) {
        felt += 10;
        ys++;
      }
      stabel += ys;
    }
    if (pl == player)
      score += 60*stabel;
    else
      score -= 60*stabel;
  }

  /* som ovenfor udfra hjoerne 4 */

  if (bd->sq[81] == (char)player || bd->sq[81] == (char)opp) {
    pl = bd->sq[81];

    start = 81;

    felt = 82;
    xs = 0;
    while (bd->sq[felt] == (char)pl) {
      felt++;
      xs++;
    }
    felt = 71;
    ys = 0;
    while (bd->sq[felt] == (char)pl) {
      felt -= 10;
      ys++;
    }
    stabel = 0;
    while ((xs >= 1 && ys > 1) || (xs > 1 && ys >= 1)) {
      start -= 9;
      stabel ++;
      felt = start + 1;
      oxs = xs - 2;
      xs = 0;
      while (bd->sq[felt] == (char)pl && (oxs-- > 0)) {
        felt++;
        xs ++;
      }
      stabel += xs;
      felt = start - 10;
      oys = ys - 2;
      ys = 0;
      while (bd->sq[felt] == (char)pl && (oys-- > 0)) {
        felt -= 10;
        ys++;
      }
      stabel += ys;
    }
    if (pl == player)
      score += 60*stabel;
    else
      score -= 60*stabel;
  }


  /* som ovenfor udfra hjoerne 3 */

  if (bd->sq[88] == (char)player || bd->sq[88] == (char)opp) {
    pl = bd->sq[88];

    start = 88;

    felt = 87;
    xs = 0;
    while (bd->sq[felt] == (char)pl) {
      felt--;
      xs++;
    }
    felt = 78;
    ys = 0;
    while (bd->sq[felt] == (char)pl) {
      felt -= 10;
      ys++;
    }
    stabel = 0;
    while ((xs >= 1 && ys > 1) || (xs > 1 && ys >= 1)) {
      start -= 11;
      stabel ++;
      felt = start - 1;
      oxs = xs - 2;
      xs = 0;
      while (bd->sq[felt] == (char)pl && (oxs-- > 0)) {
        felt--;
        xs ++;
      }
      stabel += xs;
      felt = start - 10;
      oys = ys - 2;
      ys = 0;
      while (bd->sq[felt] == (char)pl && (oys-- > 0)) {
        felt -= 10;
        ys++;
      }
      stabel += ys;
    }
    if (pl == player)
      score += 60*stabel;
    else
      score -= 60*stabel;
  }

/*  comdis = bd->ndiscs[computer];
  humdis = bd->ndiscs[human];
  dissco = (int)((long)(ndisc - 40)*300l / 40);

  score += (short int)(dissco*(comdis - humdis))/(comdis + humdis + 2); */

  /* her undersoges, om vores score ligger saa langt udenfor vores */
  /* alfa-beta vindue, at scoren for mobiliteten ikke kan bringe   */
  /* den indenfor dette vindue igen. Hvis dette er tilfaeldet, da  */
  /* beregnes scoren for mobiliteten ikke. Dette sparer en del tid */
  /* da mobiliteten kraever en del beregning, og da scoren for     */
  /* stabilitet i mange tilfaelde er langt stoerre end scoren for  */
  /* mobilitet                                                     */
	
    if ((score + CURMOB + POTMOB <= alfa) || (score - CURMOB - POTMOB >= beta))
      return score; 

   commov = (int)countmov(player, bd, potential);
   hummov = (int)plmov;
 
  /* her beregnes scoren for mobilitet */
  /* som beskrevet i rapporten analogt til paul rosenbloom */

  score += (short int)((CURMOB*(commov - hummov))/(commov + hummov + 2));
  commov = (int)potential[player];
  hummov = (int)potential[opp];
  score += (short int)((POTMOB*(commov - hummov))/(commov + hummov + 2));

  return score;
}

