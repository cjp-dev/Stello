/*************************************************************************/
/*                                                                       */
/*     tree.c, algoritmer til administration af det gemte spiltrae       */
/*                                                                       */
/*************************************************************************/
#include "stdafx.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "reversi.h"

extern tree *root,*nfree,*fnode;
extern long aalloc,maxalloc;


/************************************************/
/* readlib konverterer biblioteket til treaform */
/************************************************/

void readlib(tree ** root,char* *src)
{
  int children;
  tree *temp;

  if ((children = *(*src)++) == 0)
    return;

  make_node(root);
  (*root)->move = *(*src)++;
  readlib(&(*root)->barn,src);

  temp = *root;
  while (--children > 0) {
    make_node(&temp->sosk);
    temp = temp->sosk;
    temp->move = *(*src)++;
    readlib(&temp->barn,src);
  }
} 

/*******************************************************************************/
/* Get_lib laeser biblioteket ind fra disken, og konverterer det saadan at de  */
/* spejlede versioner bliver dannet                                            */
/*******************************************************************************/

/* int Get_lib(char * whitelib, char * blacklib)
{
  char buffer[6000],*src;
  FILE * f;
  tree * temp,*temp1;

  if ((f = fopen(blacklib,"r")) != NULL) {
    if (fread(&buffer,1,sizeof(buffer), f) > 0) {
      src = buffer;
      readlib(&blackroot,&src);
      temp1 = blackroot;
      while (temp1->move != 53)
        temp1 = temp1->sosk;
      temp1 = temp1->barn;
      while (temp1->move != 66)
        temp1 = temp1->sosk;
      temp1 = temp1->barn;
      while (temp1->move != 56)
        temp1 = temp1->sosk;
      temp1 = temp1->barn;
      while (temp1->move != 65)
        temp1 = temp1->sosk;

      temp = blackroot;
      while (temp->move != 53)
        temp = temp->sosk;
      temp = temp->barn;
      while (temp->move != 65)
        temp = temp->sosk;
      temp = temp->barn;
      while (temp->move != 56)
        temp = temp->sosk;
      temp = temp->barn;
      while (temp->move != 66)
        temp = temp->sosk;
      temp->barn = temp1->barn;
    }
    else
      return FALSE;
  }
  else
    return FALSE;

  if ((f = fopen(whitelib,"r")) != NULL) {
    if (fread(&buffer,1,sizeof(buffer), f) > 0) {
      src = buffer;
      readlib(&whiteroot,&src);
      temp1 = whiteroot;
      while (temp1->move != 53)
        temp1 = temp1->sosk;
      temp1 = temp1->barn;
      while (temp1->move != 66)
        temp1 = temp1->sosk;
      temp1 = temp1->barn;
      while (temp1->move != 56)
        temp1 = temp1->sosk;
      temp1 = temp1->barn;
      while (temp1->move != 65)
        temp1 = temp1->sosk;

      temp = whiteroot;
      while (temp->move != 53)
        temp = temp->sosk;
      temp = temp->barn;
      while (temp->move != 65)
        temp = temp->sosk;
      temp = temp->barn;
      while (temp->move != 56)
        temp = temp->sosk;
      make_node(&temp->barn);
      temp = temp->barn;
      temp->move = 66;
      temp->barn = temp1->barn;
    }
    else
      return FALSE;
  }
  else
    return FALSE;
        
  maxalloc -= aalloc;
  aalloc = 0; 
  return TRUE;
} */

/**************************************************************************/
/* init_nodes initialiserer vores liste af frie knuder. De opbevares i en */
/* hukkommelses blok.                                                     */
/**************************************************************************/

void init_nodes(long antal)
{
  tree * nknude, *sidste;

  nfree = (struct tnode *)malloc(antal*sizeof(tree));
  maxalloc = antal - 40; 
  /* vi soerger for at der er plads til mindst 40 treak i sidste knude */
  
  aalloc = 0;

  nknude = nfree;
  sidste = nfree + antal - 1;
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
void put_in_front(tree* *start, tree* *cur)
{
    tree *temp,*temp1;
    
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

/*******************************************************************/
/* list_in_tree tager en treakliste og indsaetter den i spiltraet  */
/*******************************************************************/
void list_in_tree(movelist *list,tree* *spiltree)
{
  long x;
  char *flis;
  tree *temp,*ftree = nfree;

  flis = (char *)list;
  /* flis peger paa det foerste element i movelist strukturen */
  
  x = *flis++;
  /* x er nu antallet af treak i listen */
  
  aalloc += x--;
  /* dette laegges til den samlede maengde af allokerede knuder */
  
  if (aalloc > maxalloc)
    /* hvis der nu ikke er plads til flere knuder saettes treeon til falsk */
    treeon = FALSE; 
          
  (*spiltree) = ftree;
  do  {
    temp = ftree;
    ftree = ftree->barn;
    temp->barn = NULL;
    temp->sosk = ftree;
    temp->move = *flis++;
  } 
  while (--x >= 0); 
  temp->sosk = NULL;
  /* vi indsaetter traekkene i listen */
  
  nfree = ftree;
  /* nfree peger nu paa naeste frie knude */
} 

/*******************************************************************************/
/* make_node laver en dummy_knude, bruges naar en spiller ikke har noget treak */
/*******************************************************************************/
void make_node(tree* *spiltree)
{
  tree *temp;

  temp = nfree; 
  (*spiltree) = temp;
  
  nfree = temp->barn;
  /* nfree peger nu paa naeste frie knude */
  
  if (++aalloc > maxalloc)
    /* hvis der nu ikke er plads til flere knuder saettes treeon til falsk */
    treeon = FALSE;
  temp->move = 0; 
  temp->barn = temp->sosk = NULL;
} 

/***************************************/
/* den rekursive version af free_nodes */
/***************************************/

void rfree_nodes(tree *rt)
{
  tree *temp;

  temp = rt->barn;
  /* temp peger paa det foerste barn af denne knude */

  while (temp != NULL) {
    /* saa laenge der er boern */

    free_nodes(temp);
    /* frigoer dette barn */

    temp = temp->sosk;
    /* temp peger nu paa naeste barn */
  }

  /* nu er alle boern frigjordt, saa vi skal frigoere selve knuden */
  rt->barn = nfree;
  /* knuden saettes til at pege paa den naeste frie knude */

  nfree = rt;
  /* poshort interen til den naeste frie knude sattes til at pege paa denne */
  /* knude                                                            */

  aalloc--;
  /* aalloc holder rede paa, hvor mange knuder vi har brugt, da vi lige */
  /* har frigjordt en, saa skal der treakkes en fra dette antal         */

} 

/***********************************************************************/
/* free_nodes er en ikke rekursiv algoritme til at gennemloebe et trae */
/* den fjerner knuderne fra traet og indsaetter dem i listen af frie   */
/* treak. Den er ca. dobbelt saa hurtig som den rekursive version.     */
/***********************************************************************/

void free_nodes(tree *rt)
{
  long x = 0;
  tree **stak, *temp[30], *ftree = nfree;
  /* vi bruger her en stak til at holde rede paa hvor langt vi er kommet */
  /* stak er stackposhort interen, og temp er selve stakken. ftree er en       */
  /* poshort inter der bruges til at opdatere de frie knuder, den opbevares i  */
  /* en variabel under selve opdateringen, saa vi kan bruge et register  */
  /* istedet for en hukommelse addresse, da det er hurtigere             */

  stak = temp;
  *stak++ = NULL;
  /* det foerste element i stakken saettes til nul, saa vi ved hvornaar */
  /* vi skal stoppe                                                     */

  do {
    if (rt != NULL) {
      /* hvis knuden eksisterer */

      *stak++ = rt;
      /* saa gem den paa stakken */

      rt = rt->barn;
      /* og gaa et niveau laengere ned i traeet */
    }
    else {
      /* vi kan ikke komme laengere ned i traeet, saa vi skal et  */
      /* niveau op for at finden den sidst brugte knude. Hvis den */
      /* er nul, saa er vi faerdige.                              */
      if ((rt = *(--stak)) == NULL)
        break;

      /* ellers slettes denne knude fra traeet ved at tilfoeje den til */
      /* listen af frie knuder                                         */
      rt->barn = ftree;
      /* knuden saettes til at pege paa den naeste frie knude */

      ftree = rt;
      /* poshort interen til den naeste frie knude sattes til at pege paa */
      /* denne knude                                                */

      x++;
      /* x holder rede paa, hvor mange knuder vi har slettet */

      rt = rt->sosk;
      /* vi fortsaetter med naeste knude paa samme niveau */
    }
  } while (TRUE);

  aalloc -= x;
  /* vi treakker antallet af slettede knuder, fra antallet af brugte */
  /* knuder                                                          */

  nfree = ftree;
  /* nfree saettes til at pege paa det foerste element i listen af frie */
  /* knuder                                                             */
}

/***********************************************************************/
/* bcopytree undersoeger om det treak som modstanderen har fortaget er */
/* det teoretisk bedste, hvis det er det slettes den del af traeet som */
/* ikke mere skal bruges, og der retuneres sand. Dette bruges af slut- */
/* spils algoritmen, hvor computeren saa blot kan finde sit treak i    */
/* traet og traekke med det samme, da den allerede har beregnet sit    */
/* bedste svar paa modstanderens treak                                 */
/***********************************************************************/

short int bcopytree(tree* *rt)
{
  tree *temp;

  if ((temp = (*rt)->barn) == NULL)
    /* hvis traeet er tomt, saa retuner falsk */
    return FALSE;

  if ((temp = temp->barn) == NULL)
    /* hvis barnet af det foerste treak ikke findes saa retuner falsk */
    return FALSE;
  if ((char)temp->move != game.moves[playnm - 1])
    /* hvis modstanderen ikke foretog det optimale treak saa retuner falsk */
    return FALSE;

  if (temp->barn == NULL)
    /* hvis svaret paa modstanderens traek ikke findes saa retuner falsk */
    return FALSE;

  (*rt)->barn->barn = (*rt)->barn->barn->sosk;
  /* vi fjerner den del af traeet hvor den optimale treakraekkefoelge */
  /* staar                                                            */

  temp->sosk = NULL;
  temp->move = 0;
  free_nodes(*rt);
  /* vi frigoeer den del af traeet vi ikke mere skal bruge */

  (*rt) = temp;
  /* vi saetter roden til at pege paa den del af traet hvor den optimale */
  /* treakraekkefoelge staar og retunerer sand                           */
  return TRUE;
}

/*************************************************************************/
/* copytree bruges til at kopiere den del af traeet vi kan bruge ved     */
/* naeste soegning. hvis vi feks har foretaget en soegning af dybde 8,   */
/* og modstanderen nu foretager det treak som computeren har beregnet    */
/* skulle vaere det bedste svar, saa starter vi den iterative soegning   */
/* med dybde 6, og med den del af traet som findes fra spiltraet fra     */
/* sidste soegning som starttrae. Dette giver en gevinst, da man slipper */
/* for at foretage de foerste soegninger, der jo allerede blev foretaget */
/* i sidste soegning.                                                    */
/*************************************************************************/

short int copytree(tree* *rt)
{
  tree *temp,*temp1;
  short int ok;

  ok = 0;

  if ((temp = (*rt)->barn) == NULL)
    /* hvis traeet er tomt, saa retuner falsk */
    return -1;

  if ((temp = temp->barn) == NULL)
    /* hvis barnet af det foerste treak ikke findes saa retuner falsk */
    return -1;

  /* temp peger nu paa modstanderens svar hvis det findes*/

  if ((char)temp->move == game.moves[playnm - 1]) {
    /* hvis dette treak var det forventede */

    (*rt)->barn->barn = (*rt)->barn->barn->sosk;
    /* vi fjerner den del af traeet hvor den forventede treakraekkefoelge */
    /* staar                                                              */

    if (temp->move != 0)
      /* vi retunerer, hvis det var et lovligt treak */
      ok = 1;
    else
      /* og 2, hvis dette treak var en overspringelse */
      ok = 2;
  }
  else {
    /* hvis treakket ikke var det forventede, saa leder vi i traet til */
    /* vi finder det sted hvor traekket staar. Vi bruger denne del af  */
    /* traet til at starte den iterative soegning med, da den er for-  */
    /* holdsvis sorteret.                                              */
    while ((char)temp->move != game.moves[playnm - 1]) {
      temp1 = temp;
      temp = temp->sosk;
    }
    temp1->sosk = temp->sosk;
  }
  temp->sosk = NULL;
  temp->move = 0;
  free_nodes(*rt);
  /* vi frigoeer den del af traeet vi ikke mere skal bruge */

  (*rt) = temp;
  /* vi saetter roden til at pege paa den del af traet hvor den optimale */
  /* treakraekkefoelge staar og retunerer ok                             */
  return ok;
}
