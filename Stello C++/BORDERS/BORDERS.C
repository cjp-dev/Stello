/*****************************************************************************/
/*                                                                           */
/*            borders.c,  program til at generere kanttabellerne             */
/*                                                                           */
/*****************************************************************************/ 

#include <stdio.h>
#include <string.h>

#define FALSK 0
#define SAND 1
#define DEPTH 6

/********************************************************/
/* definitioner for tilstandene af et felt paa braedtet */
/* felterne skal defineres som absolutte vaerdier, da   */
/* de vi bruger vaerdien af et felt til indexberegning  */
/********************************************************/

#define COM 0
#define MEN 1
#define TOM 2
#define modstander( x )  (1 - x)


#define KANTER 6561     /* 3 i 8 forskellige kanter */

/*********************************************************/
/*   et felt kan vaere i en af foelgende fire tilstande  */
/*********************************************************/

enum stabilitet {stabil, semistabil, ustabil, tom};

/**************************************************/
/*   kanten af et braedt har 8 felter, dette      */
/*   implementeres som en array                   */
/**************************************************/

typedef short int kant[8];
short int brikstatus(kant k, int felt);

/*****************************************************/
/*  vaerdier for brikker der er henholdsvis;         */
/*         stabile     (stascor)                     */
/*         semistabile (semiscor)                    */
/*      og ustabile    (unstscor)                    */
/*  nuvkant er den kant vi er i gang med at beregne  */
/*****************************************************/

kant stascor = {128,136,124,102,102,124,136,128},
     semistab = {0,-40,10,5,5,10,-40,0},
     unstscor = {0,-64,2,1,1,2,-64,0},
     nuvkant;

/*******************************************************/
/* hjotrk er en array der for hver mulig kant viser    */
/* om henholdsvis mennesket og maskinen har et muligt  */
/* hjornetraek, og om det kan vaere farligt at lave    */
/* dette hjornetraek. Strukturen for et array felt er  */
/* som folger;                                         */
/*   struct {                                          */
/*     unsigned farlig hjorne 1 for menneske    : 1,   */
/*              farlig hjorne 2 for menneske    : 1,   */
/*              farlig hjorne 1 for computer    : 1,   */
/*              farlig hjorne 2 for computer    : 1,   */
/*              menneske kan gore hjornetraek 1 : 1,   */
/*              menneske kan gore hjornetraek 2 : 1,   */
/*              computer kan gore hjornetraek 1 : 1,   */
/*              computer kan gore hjornetraek 2 : 1,   */
/*     } felt;                                         */
/*                                                     */
/* for at lette forstaelsen er der defineret til-      */
/* horende markroer                                    */
/*******************************************************/

#define MIDT_HUMAN 0x2000
#define MIDT_COMPUTER 0x1000
#define SIKKER_HVID_HJ1 0x800
#define SIKKER_HVID_HJ2 0x400
#define SORT_HJ1 0x200
#define SORT_HJ2 0x100
#define FJ1M 0x80
#define FJ2M 0x40
#define FJ1C 0x20
#define FJ2C 0x10
#define MHT1 0x08
#define MHT2 0x04
#define CHT1 0x02
#define CHT2 0x01

short int hjotrk[KANTER];

/*********************************************************/
/* de fire arrays basis, midt, venst og hoej, indeholer  */
/* vaerdierne for en kant, hvor det gaelder henholdsvis; */
/* basis = vaerdi nar der ikke saettes en brik paa kant  */
/* midt = vaerdi nar der saettes en brik internt i kant  */
/* venst = vaerdi nar der saettes en brik paa venstre    */
/*         hjoernefelt                                   */
/* hoej = vaerdi nar der saettes en brik paa hoejre      */
/*         hjoernefelt                                   */
/*********************************************************/

short int basis[KANTER],sikker[KANTER],midt[KANTER],
					venst[KANTER],hoej[KANTER],
					cmindex[KANTER],cvindex[KANTER],chindex[KANTER],hvindex[KANTER],hhindex[KANTER],cmidt[KANTER],hmidt[KANTER];

typedef kant kanter[KANTER];

#define MAXMOVES 9
typedef struct {
            char nmoves;
            char move[MAXMOVES];
            } movelist;

long calcnodes,cornernodes;

/**************************************************************/
/* status bruges til at gemme brik/felt vaerdierne i til test */
/**************************************************************/

kanter status;

/*********************************************************/
/* udskrift funktion til at teste vaerdierne i kanterne  */
/*********************************************************/

void skrivstatus(FILE *f,kant c)
{
  int felt;

  for (felt = 0; felt <= 7; felt++)
    if (c[felt] == stabil)
      fprintf(f,"st ");
    else if (c[felt] == semistabil)
      fprintf(f,"ss ");
    else if (c[felt] == ustabil)
      fprintf(f,"us ");
    else
      fprintf(f,"to ");
}

void skrivbit(FILE *f,int word)
{ int x;

  for (x = 0; x <= 15; x++) {
    if (word & 32768)
      fprintf(f,"1");
    else
      fprintf(f,"0");
    word <<= 1;
  }
}

void skrivkant(FILE *f,kant k)
{
    int felt;

    for (felt = 0; felt <= 7; felt++)
      switch (k[felt]) {
        case 0: fprintf(f,"o");  break;
        case 1: fprintf(f,"x");  break;
        case 2: fprintf(f,"-");  break;
      }
}

/**********************************************************/
/* funktionen indx omregner en kant til et index. En kant */
/* betragtes som 3'tals system. Dette omregnes saa til    */
/* decimalt index.                                        */
/**********************************************************/
int indx(kant t1)
{
  short int * t = (short int *)t1;
  int r;

  r = (*(t++));
  r *= 3;
  r += (*(t++));
  r *= 3;
  r += (*(t++));
  r *= 3;
  r += (*(t++));
  r *= 3;
  r += (*(t++));
  r *= 3;
  r += (*(t++));
  r *= 3;
  r += (*(t++));
  r *= 3;
  r += (*(t));

  return r;
}


/* #define kopier(des, src) { \
  long *d,*s; \
  d = (long *)des; \
  s = (long *)src; \
  *(d++) = *(s++); \
  *(d++) = *(s++); \
  *(d++) = *(s++); \
  *(d++) = *(s++); \
} \  */

void kopier(kant des, kant src)
{
  short int z,*d,*s;
  d = (short int *)des;
  s = (short int *)src;
  z = 8;
  do
    *(d++) = *(s++);
  while (--z);
} 

/*********************************************************/
/* funktionen hjo_trk_1_muligt undersoger om spiller     */
/* udmiddelbart! kan lave et hjorne treak i hjorne 1     */
/*********************************************************/

int hjo_trk_1_muligt(kant k,int spiller)
{
  int felt,opp;

  if (k[0] == TOM) {
    opp = modstander(spiller);
    if (k[1] == opp) {
      felt = 2;
      while ((felt < 7) && (k[felt] == opp)) felt++;
      if (k[felt] == spiller)
        return SAND;
    }
  }
  return FALSK;
}

/*********************************************************/
/* funktionen hjo_trk_2_muligt undersoger om spiller     */
/* udmiddelbart! kan lave et hjorne treak i hjorne 2     */
/*********************************************************/

int hjo_trk_2_muligt(kant k,int spiller)
{
  int felt,opp;

  if (k[7] == TOM) {
    opp = modstander(spiller);
    if (k[6] == opp) {
      felt = 5;
      while ((felt > 0) && (k[felt] == opp)) felt--;
      if (k[felt] == spiller)
        return SAND;
    }
  }
  return FALSK;
}

/**********************************************************/
/* funktionen trymove udfoerer et kanttreak, og retunerer */
/* SAND hvis det var et legalt treak ud fra denne kant-   */
/* tilstand, altsaa ved at vende brikker paa kanten.      */
/**********************************************************/

int trymove(kant k,int felt,int spiller)
{
  int f1,opp,legal;

  opp = modstander(spiller);
  legal = FALSK;
  k[felt] = spiller;

  if (felt > 1) {
    f1 = felt - 1;
    if (k[f1] == opp) {
      while ((--f1 > 0) && (k[f1] == opp));
      if (k[f1] == spiller) {
        legal = SAND;
        f1++;
        do
          k[f1++] = spiller;
        while (k[f1] != spiller);
      }
    }
  }

  if (felt < 6) {
    f1 = felt + 1;
    if (k[f1] == opp) {
      while ((++f1 < 7) && (k[f1] == opp));
      if (k[f1] == spiller) {
        legal = SAND;
        f1--;
        do
          k[f1--] = spiller;
        while (k[f1] != spiller);
      }
    }
  }
  return legal;
}

/*********************************************************/
/* funktionen hjo_trk_1_sikkert undersoger om spiller    */
/* kan lave et hjorne treak i hjorne 1 uanset hvad mod-  */
/* standeren kan gore i traekket foer.                   */
/*********************************************************/

int hjo_trk_1_sikkert(kant k,int spiller)
{
  int felt,opp,z;
  kant temp;

  if (k[0] == TOM) {
    opp = modstander(spiller);
    if (k[1] == opp) {
      felt = 2;
      while ((felt < 7) && (k[felt] == opp)) felt++;
      if (k[felt] == spiller) {
        while ((felt < 7) && (k[felt] == spiller)) felt++;
        if (k[felt] == TOM) {
          if (felt < 7) {
            for (z = 0; z <= 7; temp[z] = k[z],z++);
            trymove(temp,felt,opp);
            if (hjo_trk_1_muligt(temp,spiller))
              return SAND;
          }
        }
        else
          return SAND;
      }
    }
  }
  return FALSK;
}

/*********************************************************/
/* funktionen hjo_trk_2_sikkert undersoger om spiller    */
/* kan lave et hjorne treak i hjorne 2 uanset hvad mod-  */
/* standeren kan gore i traekket foer.                   */
/*********************************************************/

int hjo_trk_2_sikkert(kant k,int spiller)
{
  int felt,opp,z;
  kant temp;

  if (k[7] == TOM) {
    opp = modstander(spiller);
    if (k[6] == opp) {
      felt = 5;
      while ((felt > 0) && (k[felt] == opp)) felt--;
      if (k[felt] == spiller) {
        while ((felt > 0) && (k[felt] == spiller)) felt--;
        if (k[felt] == TOM) {
          if (felt > 0) {
            for (z = 0; z <= 7; temp[z] = k[z],z++);
            trymove(temp,felt,opp);
            if (hjo_trk_2_muligt(temp,spiller))
              return SAND;
          }
        }
        else
          return SAND;
      }
    }
  }
  return FALSK;
}

/*********************************************************/
/* funktionen venstab afgoerer om brikkerne er "staerkt" */
/* stabile fra venstre side.                             */
/*********************************************************/

int venstab(kant k, int felt, int spiller)
{
  while (--felt >= 0)
    if (k[felt] != spiller)
      return FALSK;
  return SAND;
}

/*********************************************************/
/* funktionen hoestab afgoerer om brikkerne er "staerkt" */
/* stabile fra hoejre side.                              */
/*********************************************************/

int hoestab(kant k, int felt, int spiller)
{
  while (++felt <= 7)
    if (k[felt] != spiller)
      return FALSK;
  return SAND;
}

/***********************************************************/
/* funktionen vendven afgoerer om brikkerne kan vendes     */
/* fra venstre side, den bruger funktionerne findvtom,     */
/* der finder naeste tomme felt mod venstre, og vendh,     */
/* der vender brikkerne fra det tomme felt mod hoejre.     */
/* Vi ved, at der paa begge sider er enten en tom eller    */
/* en modstanderbrik. Vi placerer saa brikker i de tomme   */
/* felter saaledes, at vi hurtigst muligt vender brikkerne */
/* mod vores testbrik. Hvis vi har brugt alle tomme felter */
/* og ikke er naaet til vores test brik, saa er den        */
/* venstre stabil.                                         */
/***********************************************************/

int findvtom(kant k, int felt)
{
  while (--felt >= 0)
    if (k[felt] == TOM)
      break;
  return felt;
}

int vendh(kant k,int felt,int spiller)
{
  int f1,opp;

  opp = modstander(spiller);
  k[felt] = spiller;

  if (felt > 1) {
    f1 = felt - 1;
    if (k[f1] == opp) {
      while ((--f1 > 0) && (k[f1] == opp));
      if (k[f1] == spiller) {
        f1++;
        do
          k[f1++] = spiller;
        while (k[f1] != spiller);
      }
    }
  }

  while (k[++felt] == opp) k[felt] = spiller;
  return felt;
}

int vendven(kant k,int felt, int spiller)
{
   int z,tomtfelt;
   kant temp;

  for (z = 0; z <= 7; temp[z] = k[z],z++);

  tomtfelt = felt;
  while ((tomtfelt = findvtom(temp,tomtfelt)) >= 0) {
    if (temp[tomtfelt+1] == spiller) {
      if (vendh(temp,tomtfelt,modstander(spiller)) > felt)
        return SAND;
    }
    else
      vendh(temp,tomtfelt,spiller);
  }
  return FALSK;
}

/*********************************************************/
/* funktionen vendhoej afgoerer om brikkerne kan vendes  */
/* fra hoejre side, den bruger funktionerne findhtom,    */
/* der finder naeste tomme felt mod hoejre, og vendv,    */
/* der vender brikkerne fra det tomme felt mod venstre.  */
/* Samme forudsaetning som foregaaende funktion.         */
/*********************************************************/

int findhtom(kant k, int felt)
{
  while (++felt <= 7)
    if (k[felt] == TOM)
      break;
  return felt;
}

int vendv(kant k,int felt,int spiller)
{
  int f1,opp;

  opp = modstander(spiller);
  k[felt] = spiller;

  if (felt < 6) {
    f1 = felt + 1;
    if (k[f1] == opp) {
      while ((++f1 < 7) && (k[f1] == opp));
      if (k[f1] == spiller) {
        f1--;
        do
          k[f1--] = spiller;
        while (k[f1] != spiller);
      }
    }
  }

  while (k[--felt] == opp) k[felt] = spiller;
  return felt;
}
int vendhoej(kant k,int felt, int spiller)
{
  int z,tomtfelt;
  kant temp;

  for (z = 0; z <= 7; temp[z] = k[z],z++);

  tomtfelt = felt;
  while ((tomtfelt = findhtom(temp,tomtfelt)) <= 7) {
    if (temp[tomtfelt-1] == spiller) {
      if (vendv(temp,tomtfelt,modstander(spiller)) < felt)
        return SAND;
    }
    else
      vendv(temp,tomtfelt,spiller);
  }
  return FALSK;
}

/***************************************************************/
/* funktionen stabel afgoerer om brikkerne er stabile. Dette   */
/* goeres i flere afsnit. Vi ved, at hjoerner er stabile, saa  */
/* dette proeves foerst. Hvis kanten er helt fuld, er alle     */
/* brikkerne stabile, dette er naeste test. Vi tester, saa om  */
/* brikken er "staerkt" venstre eller hoejre stabil. Dette     */
/* vil sige at alle brikkerne, fra den testede brik, mod enten */
/* venstre eller hoejre, ud til hjornet ogsaa er spiller-      */
/* brikker. Hvis dette gaelder kan brikken ikke vendes.        */
/* Det sidste tilfaelde hvor brikker er stabile er, hvis de    */
/* ikke kan vendes, fordi der ikke er tomme felter nok til at  */
/* vende dem. Dette skal gaelde for baade venstre og hoejre    */
/* side.                                                       */
/***************************************************************/

int stabel(kant k, int felt)
{ int spiller = k[felt];
  if ((felt == 0) || (felt == 7))
    return SAND;   /* hjoerner er stabile */

  else if (((k[0] != TOM) && (k[1] != TOM) && (k[2] != TOM)
          && (k[3] != TOM) && (k[4] != TOM) && (k[5] != TOM)
          && (k[6] != TOM) && (k[7] != TOM)))
    return SAND; /* hvis alle felter optaget saa er felt stabil */

  else if (venstab(k,felt,spiller))
    return SAND; /* "staerkt" stabil mod venstre ? */

  else if (hoestab(k,felt,spiller))
    return SAND; /* "staerkt" stabil mod hoejre ? */

  else if (!vendven(k,felt,spiller) && !vendhoej(k,felt,spiller))
    return SAND; /* hvis brikken ikke kan vendes mod hverken */
                 /* venstre eller hoejre saa stabil          */
  else
    return FALSK;/* brik ikke stabil */
}

/**************************************************************/
/* funktionen semistabel afgoerer om brikkerne er semistabile */
/* brikkerne er semistabile, hvis de ikke kan vendes i naeste */
/* treak. Dette gaelder hvis de to fjernste nabofelter, til   */
/* hver side, er enten tomme eller modstanderbrikker.         */
/* Naar denne funktion kaldes, er alle stabile brikker fra-   */
/* sorteret. Vi ved altsaa, at der pa begge sider er enten en */
/* tom eller en modstander brik. Dette goer det lettere at    */
/* programere funktionen, vi skal blot finde de to nabofelter */
/* forskellig fra computerbrik, og hvis de er ens, saa er     */
/* computerbrikken semistabil.                                */
/**************************************************************/

int nvenst(kant k,int felt, int spiller)
{
   while (k[--felt] == spiller);
   return k[felt];
}

int nhoej(kant k,int felt, int spiller)
{
   while (k[++felt] == spiller);
   return k[felt];
}

int semistabel(kant k,int felt)
{ int spiller = k[felt];
  if (nvenst(k,felt,spiller) == nhoej(k,felt,spiller))
    return SAND;
  else
    return FALSK;
}

/***************************************************************/
/* funktionen brikstatus retunerer status af brik paa felt     */
/* Hvis vi vil have status af en brik, saa skal vi kalde denne */
/* funktion, da semistabel kun virker, hvis stabel har varet   */
/* kaldt foerst. Semistabel ved nemlig, at der paa begge sider */
/* af brikken vil vaere enten en tom, eller en modstanderbrik. */
/***************************************************************/

short int brikstatus(kant k, int felt)
{
  if (stabel(k,felt))
    return stabil;
  else if (semistabel(k,felt))
    return semistabil;
  else
    return ustabil;
}

/*********************************************************/
/* funktionen status tildeler vaerdier til enhver basis  */
/* kant, dette goeres ved at afgoere om brikkerne er     */
/* stabile, semistabile eller ustabile og addere de til- */
/* svarende vaerdier.                                    */
/*********************************************************/

short int kantvaerdi(kant k, int index)
{
  short int felt,vaerdi,bstatus;

  vaerdi = 0;


  for (felt = 0; felt <= 7; felt++) {
    if (k[felt] != TOM) {
      bstatus = brikstatus(k,felt);
      status[index][felt] = bstatus;
      if (k[felt] == COM) {
        if (bstatus == stabil)
          vaerdi += stascor[felt];
        else if (bstatus == semistabil)
          vaerdi += semistab[felt];
        else
          vaerdi += unstscor[felt];
      }
      else { /* k[felt] = MEN */
        if (bstatus == stabil)
          vaerdi -= stascor[felt];
        else if (bstatus == semistabil)
          vaerdi -= semistab[felt];
        else
          vaerdi -= unstscor[felt];
      }
    }
    else
      status[index][felt] = tom;
  }
  return vaerdi;
}

/**********************************************************/
/* funktionen prop beregner sandsyneligheden for at et    */
/* treak kunne udfoeres. Dette goeres ud fra antallet af  */
/* menneskets nabo brikker.                               */
/**********************************************************/

long prop(kant k, int felt, int opp)
{
  int sandsyn = 0;

  if ((felt > 0) && k[felt - 1] == opp)
    if (felt == 1)
      sandsyn += 154;
    else
      sandsyn += 256;

  if ((felt > 1) && k[felt - 2] == opp)
    if (felt == 2)
      sandsyn += 51;
    else
      sandsyn += 154;

  if ((felt < 7) && k[felt + 1] == opp)
    if (felt == 6)
      sandsyn += 154;
    else
      sandsyn += 256;

  if ((felt < 6) && k[felt + 2] == opp)
    if (felt == 5)
      sandsyn += 51;
    else
      sandsyn += 154;

  return (long)sandsyn;
}

/**********************************************************/
/* funktionen findmax/min tildeler vaerdier til kanter    */
/* hvor computeren udfoerer et treak i midten af kanten.  */
/* Altsaa alle kanttreak minus hjoerne treak. Vi tillader */
/* kun treak hvor den placerede brik er enten stabil      */
/* eller semistabil. Hvis treakket er et legalt kanttreak */
/* d.v.s at treakket kan goeres ved at vende brikker paa  */
/* kanten, saa tages vaerdien udmiddelbart fra basis,     */
/* ellers saa beregnes vaerdien ud fra sandsyneligheden   */
/* for at dette treak kunne goeres.                       */
/* Vi har altsaa en minmaxsoegning med sandsyneligheder.  */
/* At vi kun tillader traek der ikke er hjoernetreak      */
/* skyldes, at vi vil lade vores selektive minimax soeg-  */
/* ning afgoere dette.                                    */
/**********************************************************/

short int findmin(kant k,int depth,int alfa, int beta);

short int findmax(kant k,int depth,int alfa, int beta)
{
  short int tomfelt,maxscore,score,nomove,i;
  long p;
  kant nykant;

  depth--;
  calcnodes++;

  if (depth == 0)
    nomove = maxscore = basis[indx(k)];
  else
    nomove = maxscore = findmin(k,depth,alfa,beta);
      /* maxscore saettes til basis, saa */
      /* det tillades ikke at treakke    */

  if (maxscore > alfa)
    alfa = maxscore;

  tomfelt = 7;                                     /* ikke hj³rne 0 */
  while (maxscore < beta) {
    while (--tomfelt)
      if (k[tomfelt] == TOM)
        break;
    if (!tomfelt)
      break;
    kopier(nykant,k);
    if (trymove(nykant,tomfelt,COM)) {
      i = indx(nykant);
      if (status[i][tomfelt] == ustabil)
        continue;
      if (depth == 0)
        score = basis[i];
      else
        score = findmin(nykant,depth,alfa,beta);
    }
    else { /* her er brikken semistabil, eneste mulighed ! */
      if ((p = prop(k,tomfelt,MEN)) == 0)
        continue;
      if (depth == 0)
        score = (short int)(((1024-p)*nomove +
                p*basis[indx(nykant)]) >> 10);
      else
        score = (short int)(((1024-p)*nomove +
                p*findmin(nykant,depth,alfa,beta)) >> 10);
    }
    if (score > maxscore) {
      maxscore = score;
      if (maxscore > alfa)
        alfa = maxscore;
    }
  }
  return maxscore;
}

short int findmin(kant k,int depth,int alfa, int beta)
{
  short int tomfelt,minscore,score,nomove,i;
  long p;
  kant nykant;

  depth--;
  calcnodes++;

  if (depth == 0)
    nomove = minscore = basis[indx(k)];
  else
    nomove = minscore = findmax(k,depth,alfa,beta);
      /* maxscore saettes til basis, saa */
      /* det tillades ikke at treakke    */
  if (minscore < beta)
    beta = minscore;

  tomfelt = 7;                                     /* ikke hj³rne 0 */
  while (minscore > alfa) {
    while (--tomfelt)
      if (k[tomfelt] == TOM)
        break;
    if (!tomfelt)
      break;
    kopier(nykant,k);
    if (trymove(nykant,tomfelt,MEN)) {
      i = indx(nykant);
      if (status[i][tomfelt] == ustabil)
        continue;
      if (depth == 0)
        score = basis[i];
      else
        score = findmax(nykant,depth,alfa,beta);
    }
    else { /* her er brikken semistabil, eneste mulighed ! */
      if ((p = prop(k,tomfelt,COM)) == 0)
        continue;
      if (depth == 0)
        score = (short int)(((1024-p)*nomove +
                      p*basis[indx(nykant)]) >> 10);
      else
        score = (short int)(((1024-p)*nomove +
                p*findmax(nykant,depth,alfa,beta)) >> 10);
    }
    if (score < minscore) {
      minscore = score;
      if (minscore < beta)
        beta = minscore;
    }
  }
  return minscore;
}

short int com_midt(kant k,int depth,int alfa, int beta)
{
  short int tomfelt,maxscore,score,nomove,i,bm;
  long p;
  kant nykant;

  depth--;
  calcnodes++;

/*  if (depth == 0) */
    nomove = maxscore = basis[indx(k)];
/*  else
    nomove = maxscore = findmin(k,depth,alfa,beta); */
      /* maxscore saettes til basis, saa */
      /* det tillades ikke at treakke    */ 

  if (maxscore > alfa)
    alfa = maxscore;

  tomfelt = 7;                                     /* ikke hj³rne 0 */
  while (maxscore < beta) {
    while (--tomfelt)
      if (k[tomfelt] == TOM)
        break;
    if (!tomfelt)
      break;
    kopier(nykant,k);
    if (trymove(nykant,tomfelt,COM)) {
      i = indx(nykant);
      if (status[i][tomfelt] == ustabil)
        continue;
      if (depth == 0)
        score = basis[i];
      else
        score = findmin(nykant,depth,alfa,beta);
    }
    else { /* her er brikken semistabil, eneste mulighed ! */
		  i = indx(nykant);
      if ((p = prop(k,tomfelt,MEN)) == 0)
        continue;
      if (depth == 0)
        score = (short int)(((1024-p)*nomove +
                p*basis[indx(nykant)]) >> 10);
      else
        score = (short int)(((1024-p)*nomove +
                p*findmin(nykant,depth,alfa,beta)) >> 10);
    }
    if (score > maxscore) {
		  bm = i;
      maxscore = score;
      if (maxscore > alfa)
        alfa = maxscore;
    }
  }
	if (maxscore > nomove + 100)
		return bm;
	else
  	return -1;
}

short int hum_midt(kant k,int depth,int alfa, int beta)
{
  short int tomfelt,minscore,score,nomove,i,bm;
  long p;
  kant nykant;

  depth--;
  calcnodes++;

/*  if (depth == 0) */
    nomove = minscore = basis[indx(k)];
/*  else
    nomove = minscore = findmax(k,depth,alfa,beta); */
      /* maxscore saettes til basis, saa */
      /* det tillades ikke at treakke    */
  if (minscore < beta)
    beta = minscore;

  tomfelt = 7;                                     /* ikke hj³rne 0 */
  while (minscore > alfa) {
    while (--tomfelt)
      if (k[tomfelt] == TOM)
        break;
    if (!tomfelt)
      break;
    kopier(nykant,k);
    if (trymove(nykant,tomfelt,MEN)) {
      i = indx(nykant);
      if (status[i][tomfelt] == ustabil)
        continue;
      if (depth == 0)
        score = basis[i];
      else
        score = findmax(nykant,depth,alfa,beta);
    }
    else { /* her er brikken semistabil, eneste mulighed ! */
			i = indx(nykant);
      if ((p = prop(k,tomfelt,COM)) == 0)
        continue;
      if (depth == 0)
        score = (short int)(((1024-p)*nomove +
                      p*basis[indx(nykant)]) >> 10);
      else
        score = (short int)(((1024-p)*nomove +
                p*findmax(nykant,depth,alfa,beta)) >> 10);
    }
    if (score < minscore) {
			bm = i;
      minscore = score;
      if (minscore < beta)
        beta = minscore;
    }
  }
	if (minscore < nomove - 100)
		return bm;
	else
  	return -1;
}


/**********************************************************/
/* funktionen hjo_1_treak tildeler vaerdier til kanter    */
/* hvor computeren udfoerer et treak i hjorne 1.          */
/**********************************************************/

short int hjo_1_treak(kant k)
{
  kant nykant;

  kopier(nykant,k);
  if (k[0] == TOM) {
    trymove(nykant,0,COM);
    return findmin(nykant,DEPTH - 1,-10000,10000);
  }
  else
    return 0;
}

/**********************************************************/
/* funktionen hjo_2_treak tildeler vaerdier til kanter    */
/* hvor computeren udfoerer et treak i hjorne 2.          */
/**********************************************************/

short int hjo_2_treak(kant k)
{
  kant nykant;

  kopier(nykant,k);
  if (k[7] == TOM) {
    trymove(nykant,7,COM);
    return findmin(nykant,DEPTH - 1,-10000,10000);
  }
  else
    return 0;
}




short int find_max(kant k,int spiller,int depth);

short int find_min(kant k,int spiller,int depth)
{
  short int tomfelt,minscore,score,opp;
  kant temp;

  depth--;
  cornernodes++;

  opp = modstander(spiller);
  if (depth == 0)
    minscore = basis[indx(k)];
  else
    minscore = find_max(k,opp,depth);

  tomfelt = 8;                                     /* ikke hj³rne 0 */
  do {    /* ikke hj³rne 7 */
    while (--tomfelt >= 0)
      if (k[tomfelt] == TOM)
        break;
    if (tomfelt < 0)
      break;
    kopier(temp,k);
    if (trymove(temp,tomfelt,spiller)) {
      if (depth == 0)
        score = basis[indx(temp)];
      else
        score = find_max(temp,opp,depth);
    }
    else
      continue;
    if (score < minscore)
      minscore = score;
  } while (SAND);

  return minscore;
}

short int find_max(kant k,int spiller,int depth)
{
  short int tomfelt,maxscore,score,opp;
  kant temp;

  depth--;
  cornernodes++;

  opp = modstander(spiller);

  if (depth == 0)
    maxscore = basis[indx(k)];
  else
    maxscore = find_min(k,opp,depth);

  tomfelt = 8;
  do {
    while (--tomfelt >= 0)
      if (k[tomfelt] == TOM)
        break;
    if (tomfelt < 0)
      break;
    kopier(temp,k);
    if (trymove(temp,tomfelt,spiller)) {
      if (depth == 0)
        score = basis[indx(temp)];
      else
        score = find_min(temp,opp,depth);
    }
    else
      continue;
    if (score > maxscore)
      maxscore = score;
  } while (SAND);

  return maxscore;
}

short int find_int_min(kant k,int spiller)
{
  short int tomfelt,minscore,score,opp;
  kant temp;

  opp = modstander(spiller);
  minscore = find_max(k,opp,4);
                              /* maxscore saettes til basis, saa */
                              /* det tillades ikke at treakke    */
  tomfelt = 0;                                     /* ikke hj³rne 0 */
  while ((tomfelt = findhtom(k,tomfelt)) < 7) {    /* ikke hj³rne 7 */
    kopier(temp,k);
    trymove(temp,tomfelt,spiller);
    if (brikstatus(temp, tomfelt) != ustabil)
        score = find_max(temp,opp,4);
      else
        continue;
    if (score < minscore)
      minscore = score;
  }
  return minscore;
}

short int find_int_max(kant k,int spiller)
{
  short int tomfelt,maxscore,score,opp;
  kant temp;

  opp = modstander(spiller);
  maxscore = find_min(k,opp,4);
                              /* maxscore saettes til basis, saa */
                              /* det tillades ikke at treakke    */
  tomfelt = 0;                                     /* ikke hj³rne 0 */
  while ((tomfelt = findhtom(k,tomfelt)) < 7) {    /* ikke hj³rne 7 */
    kopier(temp,k);
    trymove(temp,tomfelt,spiller);
    if (brikstatus(temp, tomfelt) != ustabil)
        score = find_min(temp,opp,4);
      else
        continue;
    if (score > maxscore)
      maxscore = score;
  }
  return maxscore;
}

/*********************************************************/
/* funktionen hjorne_1_farligt undersoger om det kan     */
/* vaere farligt at lave et hjorne treak i hjorne 1      */
/* Dette goeres ved at undersoege forskellen i scoren    */
/* mellem hvis spilleren intet treak foretager sig og    */
/* hvis han foretager et hjoernetreak. Det foerste svar  */
/* treak skal vaere et internt treak, derefter er alle   */
/* legale treak tilladt. Dette testes ved at udfoere en  */
/* minimax soegning.                                     */
/*********************************************************/

int hjorne_1_farligt(kant k, int pl)
{
  short int cornerreply,reply,opp;
  kant nykant,temp;

  opp = modstander(pl);

  if (pl == COM) {
    if (k[0] == TOM) {
      kopier(nykant,k);
      reply = find_int_min(nykant,opp);

      kopier(nykant,k);
      trymove(nykant,0,pl);
      kopier(temp,nykant);
      cornerreply = find_int_min(nykant,opp);

      if ((cornerreply + 300) < reply) {
        hjotrk[indx(temp)] |= FJ1C;
				if (k[7] == COM) 
					sikker[indx(k)] = cornerreply*75/100;
        return SAND;
      }
    }
    return FALSK;
  }
  else {
    if (k[0] == TOM) {
      kopier(nykant,k);
      reply = find_int_max(nykant,opp);

      kopier(nykant,k);
      trymove(nykant,0,pl);
      kopier(temp,nykant);
      cornerreply = find_int_max(nykant,opp);

      if ((cornerreply - 300) > reply) {
        hjotrk[indx(temp)] |= FJ1M;
				if (k[7] == MEN) 
					sikker[indx(k)] = cornerreply*75/100;

        return SAND;
      }
    }
    return FALSK;
  }
}

/*********************************************************/
/* funktionen hjorne_2_farligt undersoger om det kan     */
/* vaere farligt at lave et hjorne treak i hjorne 2      */
/*********************************************************/

int hjorne_2_farligt(kant k, int pl)
{
  short int  cornerreply,reply,opp;
  kant nykant,temp;

  opp = modstander(pl);

  if (pl == COM) {
    if (k[7] == TOM) {
      kopier(nykant,k);
      reply = find_int_min(nykant,opp);

      kopier(nykant,k);
      trymove(nykant,7,pl);
      kopier(temp,nykant);
      cornerreply = find_int_min(nykant,opp);

      if ((cornerreply + 300) < reply) {
        hjotrk[indx(temp)] |= FJ2C;
				if (k[0] == COM) 
					sikker[indx(k)] = cornerreply*75/100;
        return SAND;
      }
    }
    return FALSK;
  }
  else {
    if (k[7] == TOM) {
      kopier(nykant,k);
      reply = find_int_max(nykant,opp);

      kopier(nykant,k);
      trymove(nykant,7,pl);
      kopier(temp,nykant);
      cornerreply = find_int_max(nykant,opp);

      if ((cornerreply - 300) > reply) {
        hjotrk[indx(temp)] |= FJ2M;
				if (k[0] == MEN) 
					sikker[indx(k)] = cornerreply*75/100;
        return SAND;
      }
    }
    return FALSK;
  }
}

/*********************************************************/
/* funktionen farlig_hjoer treakker noget fra de farlige */
/* kanter                                                */
/*********************************************************/

void farlig_hjoer(kant k, int index)
{
	return;
  if (hjotrk[index] & FJ1C) {
    if (k[0] == TOM) {
      sikker[index] -= 50;
      midt[index] -= 50;
      venst[index] -= 50;
    }
    else {
      sikker[index] -= 100;
      midt[index] -= 100;
      venst[index] -= 100;
    }
  }
  if (hjotrk[index] & FJ2C) {
    if (k[7] == TOM) {
      sikker[index] -= 50;
      midt[index] -= 50;
      hoej[index] -= 50;
    }
    else {
      sikker[index] -= 100;
      midt[index] -= 100;
      hoej[index] -= 100;
    }
  }
}



int main (int argc, const char *argv[])
{
  FILE *f;
  int index,mindex,felt1,felt2,felt3,felt4,felt5,felt6,felt7,felt8,far_hjoer;
  int test_ud = FALSK, asm_ud = FALSK, c_ud = FALSK,argpoi;
	kant nykant;


  if (argc > 2) {
    printf("For mange parametre\n");
    return 0;
  }
  else if (argc < 2) {
    printf("/**************************************************/\n");
    printf("/* Programmet kan kaldes med foelgende parametre: */\n");
    printf("/*       a  =  kant tabel i assembler form        */\n");
    printf("/*       c  =  kant tabel i c form                */\n");
    printf("/*       t  =  test fil med alle kanter og        */\n");
    printf("/*             tilhoerende vaerdier               */\n");
    printf("/**************************************************/\n");
    return 0;
  }
  else {
    argpoi = 0;
    while (argv[1][argpoi] != 0) {
      switch (argv[1][argpoi])
      {
        case 'a' : asm_ud = SAND; break;
        case 'c' : c_ud = SAND; break;
        case 't' : test_ud = SAND; break;
        default : printf("Forkert parameter\n"); return 0;
      }
      argpoi++;
    }
  }

  printf("Basis tabel beregnes.\n");

  index = 0;

  for (felt1 = COM; felt1 <= TOM; felt1++) {
    nuvkant[0] = felt1;
    for (felt2 = COM; felt2 <= TOM; felt2++) {
      nuvkant[1] = felt2;
      for (felt3 = COM; felt3 <= TOM; felt3++) {
        nuvkant[2] = felt3;
        for (felt4 = COM; felt4 <= TOM; felt4++) {
          nuvkant[3] = felt4;
          for (felt5 = COM; felt5 <= TOM; felt5++) {
            nuvkant[4] = felt5;
            for (felt6 = COM; felt6 <= TOM; felt6++) {
              nuvkant[5] = felt6;
              for (felt7 = COM; felt7 <= TOM; felt7++) {
                nuvkant[6] = felt7;
                for (felt8 = COM; felt8 <= TOM; felt8++) {
                  nuvkant[7] = felt8;

                  basis[index] = kantvaerdi(nuvkant,index);
                  if (hjo_trk_1_sikkert(nuvkant,MEN))
                    hjotrk[index] |= MHT1;
                  if (hjo_trk_2_sikkert(nuvkant,MEN))
                    hjotrk[index] |= MHT2;
                  if (hjo_trk_1_muligt(nuvkant,COM))
                    hjotrk[index] |= CHT1;
                  if (hjo_trk_2_muligt(nuvkant,COM))
                    hjotrk[index] |= CHT2;
										
									if (hjo_trk_1_sikkert(nuvkant,COM))
                    hjotrk[index] |= SIKKER_HVID_HJ1;
                  if (hjo_trk_2_sikkert(nuvkant, COM))
                    hjotrk[index] |= SIKKER_HVID_HJ2;
                  if (hjo_trk_1_muligt(nuvkant, MEN))
                    hjotrk[index] |= SORT_HJ1;
                  if (hjo_trk_2_muligt(nuvkant, MEN))
                    hjotrk[index] |= SORT_HJ2;

										
									kopier(nykant, nuvkant);
									trymove(nykant,0,COM);
									cvindex[index] = indx(nykant);
									
									kopier(nykant, nuvkant);
									trymove(nykant,7,COM);
									chindex[index] = indx(nykant);
									
									kopier(nykant, nuvkant);
									trymove(nykant,0,MEN);
									hvindex[index] = indx(nykant);
									
									kopier(nykant, nuvkant);
									trymove(nykant,7,MEN);
									hhindex[index] = indx(nykant);

                  index++;
                }
              }
            }
          }
        }
      }
    }
  }

  printf("Tabeller beregnes. ");

  calcnodes = 0;

  index = 0;
  for (felt1 = COM; felt1 <= TOM; felt1++) {
    nuvkant[0] = felt1;
    for (felt2 = COM; felt2 <= TOM; felt2++) {
      nuvkant[1] = felt2;
      for (felt3 = COM; felt3 <= TOM; felt3++) {
        nuvkant[2] = felt3;
        for (felt4 = COM; felt4 <= TOM; felt4++) {
          nuvkant[3] = felt4;
          for (felt5 = COM; felt5 <= TOM; felt5++) {
            nuvkant[4] = felt5;
            for (felt6 = COM; felt6 <= TOM; felt6++) {
              nuvkant[5] = felt6;
              for (felt7 = COM; felt7 <= TOM; felt7++) {
                nuvkant[6] = felt7;
                for (felt8 = COM; felt8 <= TOM; felt8++) {
                  nuvkant[7] = felt8;

                  sikker[index] = findmin(nuvkant,DEPTH,-10000,10000);
									if ((mindex = com_midt(nuvkant,DEPTH,-10000,10000)) != -1) {
										cmidt[index] = mindex;
										hjotrk[index] |= MIDT_COMPUTER;
									}
									else
										cmidt[index] = index;
									
									if ((mindex = hum_midt(nuvkant,DEPTH,-10000,10000)) != -1) {
										hmidt[index] = mindex;
										hjotrk[index] |= MIDT_HUMAN;
									}
									else
										hmidt[index] = index;

									
                  midt[index] = findmax(nuvkant,DEPTH,-10000,10000);
                  venst[index] = hjo_1_treak(nuvkant);
                  hoej[index] = hjo_2_treak(nuvkant);
                  index++;
                }
              }
            }
          }
        }
      }
    }
  }
  printf(" Antal knuder = %ld\n",calcnodes);
  printf("Hjoerne tabel beregnes. ");
  cornernodes = 0;
  index = 0;
  far_hjoer = 0;
  for (felt1 = COM; felt1 <= TOM; felt1++) {
    nuvkant[0] = felt1;
    for (felt2 = COM; felt2 <= TOM; felt2++) {
      nuvkant[1] = felt2;
      for (felt3 = COM; felt3 <= TOM; felt3++) {
        nuvkant[2] = felt3;
        for (felt4 = COM; felt4 <= TOM; felt4++) {
          nuvkant[3] = felt4;
          for (felt5 = COM; felt5 <= TOM; felt5++) {
            nuvkant[4] = felt5;
            for (felt6 = COM; felt6 <= TOM; felt6++) {
              nuvkant[5] = felt6;
              for (felt7 = COM; felt7 <= TOM; felt7++) {
                nuvkant[6] = felt7;
                for (felt8 = COM; felt8 <= TOM; felt8++) {
                  nuvkant[7] = felt8;

                  if (hjorne_1_farligt(nuvkant,MEN)) {
                    hjotrk[index] |= FJ1M;
                    far_hjoer++;
                  }
                  if (hjorne_2_farligt(nuvkant,MEN)) {
                    hjotrk[index] |= FJ2M;
                    far_hjoer++;
                  }
                  if (hjorne_1_farligt(nuvkant,COM)) {
                    hjotrk[index] |= FJ1C;
                    far_hjoer++;
                  }
                  if (hjorne_2_farligt(nuvkant,COM)) {
                    hjotrk[index] |= FJ2C;
                    far_hjoer++;
                  }
                  index++;
                }
              }
            }
          }
        }
      }
    }
  }

  index = 0;
  for (felt1 = COM; felt1 <= TOM; felt1++) {
    nuvkant[0] = felt1;
    for (felt2 = COM; felt2 <= TOM; felt2++) {
      nuvkant[1] = felt2;
      for (felt3 = COM; felt3 <= TOM; felt3++) {
        nuvkant[2] = felt3;
        for (felt4 = COM; felt4 <= TOM; felt4++) {
          nuvkant[3] = felt4;
          for (felt5 = COM; felt5 <= TOM; felt5++) {
            nuvkant[4] = felt5;
            for (felt6 = COM; felt6 <= TOM; felt6++) {
              nuvkant[5] = felt6;
              for (felt7 = COM; felt7 <= TOM; felt7++) {
                nuvkant[6] = felt7;
                for (felt8 = COM; felt8 <= TOM; felt8++) {
                  nuvkant[7] = felt8;

                  farlig_hjoer(nuvkant,index);

                  index++;
                }
              }
            }
          }
        }
      }
    }
  }

  printf(" Antal knuder = %ld\n",cornernodes);
  if (test_ud) {
    if ((f = fopen("test.asc", "w")) == NULL)
      return 0;
    printf("Test fil laves.\n");
    index = 0;

    for (felt1 = COM; felt1 <= TOM; felt1++) {
      nuvkant[0] = felt1;
      for (felt2 = COM; felt2 <= TOM; felt2++) {
        nuvkant[1] = felt2;
        for (felt3 = COM; felt3 <= TOM; felt3++) {
          nuvkant[2] = felt3;
          for (felt4 = COM; felt4 <= TOM; felt4++) {
            nuvkant[3] = felt4;
            for (felt5 = COM; felt5 <= TOM; felt5++) {
              nuvkant[4] = felt5;
              for (felt6 = COM; felt6 <= TOM; felt6++) {
                nuvkant[5] = felt6;
                for (felt7 = COM; felt7 <= TOM; felt7++) {
                  nuvkant[6] = felt7;
                  for (felt8 = COM; felt8 <= TOM; felt8++) {
                    nuvkant[7] = felt8;

					fprintf(f,"%5d%5d%5d%5d%5d%5d%5d%5d%5d%5d  ",
						index,hvindex[index],hhindex[index],hmidt[index],cmidt[index],
						basis[index],sikker[index],
            midt[index],venst[index],hoej[index]);
            skrivbit(f,hjotrk[index]);
            fprintf(f,"  ");
            skrivstatus(f,status[index]);
            skrivkant(f,nuvkant);
            fprintf(f,"\n");

                    index++;
                  }
                }
              }
            }
          }
        }
      }
    }
    fprintf(f,"\n");
    fprintf(f,"\n");
    fprintf(f,"Antal farlige hjoerner = %5d\n",far_hjoer);
    fclose(f);
  }



  if (asm_ud) {
    if ((f = fopen("borders.s", "w")) == NULL)
      return 0;
    printf("Assembler kant tabel laves.\n");
    fprintf(f,"bornomo:");
    for (index = 0; index <= 6560; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n dc.w %1d",sikker[index]);
      else
        fprintf(f,",%1d",sikker[index]);
    }
    fprintf(f,"\n\n");
    fprintf(f,"bormo:");
    for (index = 0; index <= 6560; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n dc.w %1d",midt[index]);
      else
        fprintf(f,",%1d",midt[index]);
    }
    fprintf(f,"\n\n");
    fprintf(f,"borlmo:");
    for (index = 0; index <= 6560; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n dc.w %1d",venst[index]);
      else
        fprintf(f,",%1d",venst[index]);
    }
    fprintf(f,"\n\n");
    fprintf(f,"borrmo:");
    for (index = 0; index <= 6560; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n dc.w %1d",hoej[index]);
      else
        fprintf(f,",%1d",hoej[index]);
    }
    fprintf(f,"\n\n");
    fprintf(f,"cormo:");
    for (index = 0; index <= 6560; index++) {
      if (index % 32 == 0)
        fprintf(f,"\n dc.b %1d",(int)hjotrk[index]);
      else
        fprintf(f,",%1d",(int)hjotrk[index]);
    }
    fclose(f);
  }

  if (c_ud) {
    if ((f = fopen("kanter.c", "w")) == NULL)
      return 0;
    printf("C kant tabel laves.\n");
    fprintf(f,"short int sikker[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,",sikker[index]);
      else
        fprintf(f,"%1d,",sikker[index]);
    }
    fprintf(f,"%1d};\n\n",sikker[6560]);
		
		fprintf(f,"short int white_m[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,", cmidt[index]);
      else
        fprintf(f,"%1d,", cmidt[index]);
    }
    fprintf(f,"%1d};\n\n", cmidt[6560]);
		
		fprintf(f,"short int black_m[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,", hmidt[index]);
      else
        fprintf(f,"%1d,", hmidt[index]);
    }
    fprintf(f,"%1d};\n\n", hmidt[6560]);


		
/*    fprintf(f,"short int midten[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,",midt[index]);
      else
        fprintf(f,"%1d,",midt[index]);
    }
    fprintf(f,"%1d};\n\n",midt[6560]);
		
    fprintf(f,"short int venstre[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,",venst[index]);
      else
        fprintf(f,"%1d,",venst[index]);
    }
    fprintf(f,"%1d};\n\n",venst[6560]);
		
    fprintf(f,"short int hoejre[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,",hoej[index]);
      else
        fprintf(f,"%1d,",hoej[index]);
    }
    fprintf(f,"%1d};\n\n",hoej[6560]); */
		
/*  new stuff */

		fprintf(f,"short int white_v[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,",cvindex[index]);
      else
        fprintf(f,"%1d,", cvindex[index]);
    }
    fprintf(f,"%1d};\n\n", cvindex[6560]);
		
    fprintf(f,"short int white_h[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,",chindex[index]);
      else
        fprintf(f,"%1d,", chindex[index]);
    }
    fprintf(f,"%1d};\n\n", chindex[6560]);

    fprintf(f,"short int black_v[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,",hvindex[index]);
      else
        fprintf(f,"%1d,", hvindex[index]);
    }
    fprintf(f,"%1d};\n\n", hvindex[6560]);
		
    fprintf(f,"short int black_h[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 16 == 0)
        fprintf(f,"\n  %1d,",hhindex[index]);
      else
        fprintf(f,"%1d,", hhindex[index]);
    }
    fprintf(f,"%1d};\n\n", hhindex[6560]);

/*  new stuff end*/
		
    fprintf(f,"short int hjo_trek[6561] = {");
    for (index = 0; index <= 6559; index++) {
      if (index % 32 == 0)
        fprintf(f,"\n  %1d,",(int)hjotrk[index]);
      else
        fprintf(f,"%1d,",(int)hjotrk[index]);
    }
    fprintf(f,"%1d};",(int)hjotrk[6560]);
    fclose(f);
  }

  return 0;
}
