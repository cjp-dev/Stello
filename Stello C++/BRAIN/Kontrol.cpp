/***************************************************************************/
/*                                                                         */
/*     kontrol.c, Algoritmer til kontrol af alfa-beta soegningen           */
/*                                                                         */
/***************************************************************************/

/* #include <curses.h> */
#include "stddef.h"
#include "stdafx.h"
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <time.h>
#include "stdlib.h"
#include "reversi.h"
#include "hash.h"
#include "book.h"
#include "mmsystem.h" 


/* #define min(a, b)       ((a) < (b) ? (a) : (b))
#define max(a, b)       ((a) > (b) ? (a) : (b)) */

short int alert(char * s,char p1,char p2);
void text_move(short int move, char *tx);
void show_bt(short int);

/*************************************/
/* tider for et treak i 1000 dele sek */
/*************************************/
long tider[15] = {60,120,330,660,1330,3330,
                  6660,13330,20000,30000,40000,
                  80000,120000,200000,1000000000l};
long rtider[15] = {100,250,500,1000,2000,5000,
                  10000,20000,30000,45000,60000,
                  120000,180000,300000l,1000000000l};
                  
/*************************************/
/* tider for et parti i 1000 dele sek */
/*************************************/
long spil_tider[15] = {1000,2000,5000,10000,20000,
                  30000,45000,60000,120000l,300000l,450000l,
                  600000l,900000l,1200000l, 1800000l};

long timesleft[100];


extern short int backthink, calclib, tryagain;
enum tid_type tid_kontrol;
short int calc,bcalc,value,bvalue,oldlook,sofar,bestmove,varlook,b1calc,allway;
short int low_min,currentbest,search_type = NORMAL_SEARCH,libon,moremoves, thinking;
long timeleft[2],timemove,lowtimemove,search_time, gam_tid,GameTid;
int hash_get_height,hash_put_height,end_hash;


/*************************************************************************/
/* calc_time bruges, naar vi spiller paa den svaerhedgrad, hvor man faar */
/* en bestemt tid til et helt spil. Her bruges den til at fordele denne  */
/* tid fornuftigt for computeren, saaledes at den bruger mest tid til    */
/* sidst i spillet                                                       */
/*************************************************************************/

void calc_time(void)
{
  long emptys,empty1s,slutcount;

  emptys = 64 - (mainboard.ndiscs[computer] + mainboard.ndiscs[human]);
  /* emptys er nu antallet af tomme felter paa braeddet */

  /* meningen er, at vi vil fordele tiden fornuftigt over et helt spil, */
  /* og vi ved at de stdste treak ingen tid bruger i forhold til et     */
  /* normalt treak. Hvis vi feks har 10 treak tilbage, og vi har 10 min */
  /* til disse, saa ville det vaere dumt at taelle de sidste treak med  */
  /* nar vi skulle beregne hvor meget tid der er til hvert treak, da de */
  /* sidste 3-5 treak ville faa tildelt 6-7 min og kun i praksis ville  */
  /* bruge 10-20 sek. Vi bruger derfor en enkelt funktion der skaerer   */
  /* disse treak vaek fra vores beregninger. Vi har 15 svaerhedsgrader, */
  /* og vi tager blot og treakker dette tal (-2) fra vores antal af     */
  /* tomme felter. Vi kan altsaa maximalt fjerne 13 tomme felter,       */
  /* hvilket vil svare til at vi fjerner 13/2 ca. = 6 treak fra vores   */
  /* beregninger paa den hojeste svaerhedsgrad, hvor der er 120 min til */
  /* et spil. Dette virker godt paa de nuvaerende implemtationer, men   */
  /* hvis computeren blev 100 gange hurtigere, saa kunne man skarer 2-4 */
  /* treak mere vaek fra beregningerne, da de saa heller naesten ingen  */
  /* tid ville tage. Man kunne nok programmere en algoritme der kunne   */
  /* tage hoejde for noget saadant. Man skulle blot undersoege hvor     */
  /* lang tid det ville tage at loese en bestemt stilling, og derudfra  */
  /* ca. beregne hvor hurtig computeren var. Dette kunne saa bruges til */
  /* ca at beregne hvor langt fra slutningen at tiden per traek blev    */
  /* lav nok til at kunne ignoreres. (saa blev der sagt ret meget om    */
  /* ret lidt :-)                                                       */

  slutcount = (lookahead - 2);
  if (slutcount > 0)
    emptys -= slutcount;
  if (emptys < 0)
    emptys = 0;
  /* emptus er nu antallet af tomme felter, der kan foere til treak der */
  /* tager vaesentlig tid                                               */

  empty1s = emptys >> 1;
  /* emptys1 er nu ca. antallet af resterende treak (der tager vaesent- */
  /* lig tid) for computeren (der kan vaere overspringelser, saadan at  */
  /* computeren skal bruge flere treak for at naa slutningen            */

  if (timeleft[computer] < 0)
    timeleft[computer] = 10;
    /* her sikres, at der kan beregnes lidt selv om tiden er gaaet */

  if (empty1s > 0)
    timemove = (timeleft[computer] / empty1s);
  else
    timemove = timeleft[computer];
    
  /* timemove er nu den gennemsnitlige treaktid, som den ville vaere */
  /* hvis vi ville have lige meget tid til hvert treak               */

  timemove = (timemove >> 2) + 3*timemove*(64 - emptys)/256;
  /* timemove bliver her modificeret, saaledes at treak i starten af  */
  /* spillet, bliver udfoert hurtigere en ved slutningen af spillet   */
  /* ved at gange den med polynomiet 1/4 + 3*brikker/256, der er ca.  */
  /* 1/4 ved starten af spillet og 1 ved slutningen af spillet, hvor  */
  /* slutningen af spillet fastslaas ud fra emptys, og altsaa godt    */
  /* kan vaere naar der mangler 3-5 treak, da disse treak ikke regnes */
  /* ved til de treak som tager tid                                   */

  lowtimemove = timemove*10/15;
  /* lowtimemove bruges saadan, at der kun fortsaettes med en soegning   */
  /* et niveau dybere, hvis hojest ca. 66 % af den tilladte tid er brugt */
}

/************************************************************************/
/* calc_end_time bruges, naar vi spiller paa den svaerhedgrad, hvor man */
/* faar en bestemt tid til et helt spil, og vi er naaet frem til slut-  */
/* spilsalgoritmen. I dette tilfaelde, maa vi bruge halvdelen af den    */
/* resterende tid paa dette treak                                       */
/************************************************************************/

void calc_end_time(void)
{
  long resttid,ny_tid;

  ny_tid = clock();
	if (tid_kontrol == tid_per_trek)
		resttid = rtider[lookahead] - (ny_tid - gam_tid);
	else
  	resttid = timeleft[computer] - (ny_tid - gam_tid);
  
  if (resttid < 0)
    resttid = 10;
	if (tid_kontrol != tid_per_trek)
  	timemove = resttid >> 1;
  /* timemove er nu halvdelen af den resterende tid */

  lowtimemove = timemove*10/15;
  /* lowtimemove bruges saadan, at der kun fortsaettes med en soegning   */
  /* et niveau dybere, hvis hojest ca. 66 % af den tilladte tid er brugt */
}

/*********************************************************************/
/* stop_nu bruges til at afgoere om der er tid til at foretage endnu */
/* en soegning.                                                      */
/*********************************************************************/

short int stop_nu(short int varlook)
{
  long ny_tid;
  
  ny_tid =  clock();

  if (tid_kontrol == sogedybde)
    return (varlook > lookahead);
  /* her soeges til en bestemt dybde */

  else if (tid_kontrol == tid_per_trek) {
		if (backthink)
			return FALSE;
		else
    	return ((ny_tid - gam_tid) > tider[lookahead]);
  /* tid for et treak. Tiderne i "tider" er paa ca. 66 % af den for-  */
  /* ventede tid, dvs at hvis tiden pr treak var sat til 60 sek, saa  */
  /* vil der kun blive soegt vidre paa et niveau, hvis den brugte     */
  /* tid er mindre end 40 sek. Denne metode giver et passende gennem- */
  /* snit                                                             */
	}
  else {
		if (backthink)
			return FALSE;
		else
    	return ((ny_tid - gam_tid) > lowtimemove);
	}
  /* lowtimemove bruges saadan, at der kun fortsaettes med en soegning   */
  /* et niveau dybere, hvis hojest ca. 66 % af den tilladte tid er brugt */
}

void calc_pos_mov(board * bd)
{
  int i,j,num;
  num = 0;

  for (i = 1; i <= 8; i++)
    for (j = 1; j <= 8; j++)
      if (bd->sq[10*i + j] == EMPTY)
        bd->possible.move[num++] = 10*i + j;
  bd->possible.nmoves = num;
}

void clear_hash(void);
short int tid_udlobet;



/************************************************************************/
/* fiksignal er en signalhandler der fanger alarmen. Den bruges til at  */
/* stoppe vores sogealgoritme, hvis der er gaaet for meget tid. Dette   */
/* kan ske, hvis det foerste treak pludsilig bruger alt for lang tid.   */
/* En modifikation er, at hvis det ikke er det foerste treak der under- */
/* soeges, saa tillades at der soeges laengere, da et saadant treak     */
/* som regel kun tager lang tid at undersoege, hvis det er bedre end    */
/* det hidtil bedste, og hvis det er det, saa vil vi godt ofre den tid. */
/************************************************************************/
/* void fiksignal (int sig, int code, struct sigcontext * scp, char * addr)
{
	int trek,nytid;
  char str[40],mess[40];

	if (sig == SIGALRM) {
		if (search_type == NORMAL_SEARCH) {
			if (sofar == 0) {
        search_time = clock();
      	search_time -= gam_tid;
				if (search_time >= timemove)
					tid_udlobet = TRUE;
				else {
				  nytid = timemove - search_time;
				  ualarm(10000*nytid, 0);
				}
			}
			else {
        search_time = clock();
      	search_time -= gam_tid;
				if (search_time >= (timemove*150/100))
					tid_udlobet = TRUE;
				else {
				  nytid = (timemove*150/100) - search_time;
				  ualarm(10000*nytid, 0);
				}
			}
		}
		else 
			tid_udlobet = TRUE;
	}
	else if (sig == SIGTSTP) {
		trek = game.moves[playnm - 1];
		text_move(trek,str);
		strcpy(mess,"Var ");
		strcat(mess, str);
		strcat(mess, "det sidste treak (j,n)? (Ok)");
		backthink = FALSE;

    search_time = clock();
    search_time -= gam_tid;
		if (alert(mess,'j','n') == 0) {
      gam_tid = clock();

			if (search_time < timemove) {
				if (search_type != EXTENTED_SEARCH)
					timemove -= search_time;
				ualarm(10000*timemove, 0);
			}
			else
	  		tid_udlobet = TRUE;
		}
		else {
	  	tid_udlobet = TRUE;
			if (search_type == EXTENTED_SEARCH)  
			  clear_hash();
		}
		show_bt(FALSE);
	}
}
     
struct sigvec tidskontrol = {fiksignal,0,0}; */
jmp_buf env; 

#define TARGET_RESOLUTION 1         // 1-millisecond target resolution
TIMECAPS tc;
UINT     wTimerRes,MYTimerID = 0,msInterval;

void CALLBACK OneShotTimer(UINT wTimerID, UINT msg, 
    DWORD dwUser, DWORD dw1, DWORD dw2) { 
		MYTimerID = 0;
    tid_udlobet = TRUE ;     // handle tasks
}

/***********************************************************************/
/* getcomputer, er den funktion der styrer selve alfa-beta algoritmen. */
/* her er foretaget forskellige optimeringer, der forklares nedenunder */
/***********************************************************************/
short int getcomputer(void)
{
  tree *trtemp;
  short int alfa,beta,num,treedepth,old_back;
  board temp;
  aknud = 0;
  aeval = 0;
  varlook = 0;
  b1calc = FALSE;
  allway = 64 - (mainboard.ndiscs[computer] + mainboard.ndiscs[human]) - 1;
//	sigvec(SIGTSTP, &tidskontrol, (struct sigvec *) 0);
  
  gam_tid = clock();
  search_time = 0;
  
  timesleft[playnm] = timeleft[computer];
  timesleft[playnm + 1] = timeleft[computer];
	posible = list.nmoves;

  HashCall = 0;
	HashHit = 0;
	TransHit = 0;

	

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) 
	{
			// Error; application can't continue.
	}

	wTimerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
	timeBeginPeriod(wTimerRes); 
	
  if (bcalc) {
    /* bcalc saettes til sand under soegningen, hvis vi har foretaget   */
    /* en fuldstaendig soegning til bunds af spiltraet under slutspils- */
    /* algoritmen                                                       */

    if (bcopytree(&root)) {
      /* hvis vi kan finde computerens svar i spiltraet, saa udfoeres */
      /* det med det samme uden at foretage en udnoedvendig soegning  */
      bestmove = root->barn->move;      
      sofar = 0;
      varlook = allway + 1;
      search_time = clock();
      search_time -= gam_tid;
      timeleft[computer] -= search_time;
			make_try(bestmove,allway + 1);
			make_res(bvalue, search_time);
	//		ualarm(0, 0);
	//		show_bt(FALSE);
			timeEndPeriod(wTimerRes);
      return bestmove;
    }
    else {
      /* ellers foretages en almindelig soegning */
      alfa = -32664;
      beta = 32664;
			clear_hash();
      free_nodes(root);
      make_node(&root);
    }
  }
  else if ((calc) && oldlook >= 2) {
    /* her undersoeges, om vi har beregnet noget i forrige treak, og hvor */
    /* meget vi kan bruge det til. "Calc" er en variabel der saettes til  */
    /* sand, hvis der er foretaget en alfa-beta soegning, og oldlook har  */
    /* vaerdien af dybden af den forrige alfa-beta soegning. Der skael    */
    /* gaelde, at dyebden af den forrige soegning, skal vaere storre end  */
    /* eller lig 2, (2, da 0 er foerste niveau, 1 er andet niverau etc. ) */
    /* for at vi kan finde det naeste treak i traeet                      */

    num = copytree(&root);
    /* num har nu vaerdien 0, hvis modstanderen ikke foretog det forven-  */
    /* tede treak, 1 hvis modstanderen foretog det forventede treak og 2  */
    /* hvis modstanderen ikke havde noget treak                           */


    if (num >= 1) {
      /* modstanderen foretog det forventede treak */

      alfa = value - 1;
      /* alfa saettes til den gamle vaerdi - 1, da vi kan starte     */
      /* med et snaevet alfa-beta vindue da vi kender den noejagtige */
      /* vaerdi for denne soegning                                   */

      if (treeon || (value > 32000)) {
        /* hvis vi har gemt hele soegetraet, eller vi har fundet en */
        /* treakraekkefoelge der afslutter spillet, saa startes med */
        /* en soegning paa oldlook - 1.                             */
        varlook = oldlook-1;

        if (value > 32000)
          alfa = value - 1;
        else
          alfa = -32664;
        /* hvis vi ikke fandt en treakraekke foelge der afsluttede */
        /* spillet, saa skal alfa saettes til den mindst mulige    */
        /* vaerdi, da vi saa ikke kan sige noget om den naeste     */
        /* vaerdi. Hvis alfa var >= 32000, saa ved vi at vi ikke   */
        /* skal lede efter noget der var daarligere end denne      */
        /* ved en soegning der er 1 dybere, saa vi kan bruge       */
        /* alfa = value - 1, hvor value er vaerdien af den forrige */
        /* soegning                                                */

        beta = 32664;
        /* vi kan ikke sige noget om hvor godt det kan blive */
        if (num == 2)
          /* hvis modstanderens treak var en overspringelse, skal vi */
          /* soege et niveau dybere, da vores minimax algoritme goer */
          /* dette                                                   */
          varlook++;
      }
      else {
        /* hvis traeet ikke har mere plads, og der ikke var fundet */
        /* en treakraekkefoelge der kunne afslutte spillet, saa    */
        /* fortsaettes med en soegning paa dybden af det gemte     */
        /* sogetrae                                                */

        trtemp = root;
        treedepth = -1;
        while (trtemp->barn != NULL) {
          trtemp = trtemp->barn;
          if (trtemp->move != 0)
            treedepth++;
        }
        /* vi finder dybden af det gemte spiltrae og bruger denne  */
        /* som ny soegedybde, idet vi husker, at hvis det gemte    */
        /* treak har vaerdien 0, saa er treakket en overspringelse */
        /* og derfor skal der ikke laegges noget til treedepth da  */
        /* vores minimaxalgoritme her oeger soegedybden med 1      */

        if (treedepth > oldlook - 1)
          treedepth = oldlook - 1;
        /* det kan vaere at der har vaeret selektiv uddybning, og  */
        /* vi vil ikke soege dybere end den gamle dybde - 1 treak  */

        if (treedepth > -1)
          varlook = treedepth;
        else
          varlook = 0;

        beta = 32664;
        alfa = -32664;
      }
    }
    else {
      /* modstanderen foretog ikke det forventede treak, saa der  */
      /* soeges med stort alfa-beta vindue, og med startdybde 0.  */
      /* Bemaerk, at selve traet fra forrige soegning bruges til  */
      /* at starte den iterative soegning, hvis det findes        */
      beta = 32664;
      alfa = -32664;
			if (num < 0) {
        free_nodes(root);
        make_node(&root);
      }
    }
    if (root->barn != NULL)
      bestmove = root->barn->move;
  }
  else {
    /* der var ikke foretaget nogle beregninger i sidste treak, saa  */
    /* vi opretter et stort alfa-beta vindue, og initialiserer vores */
    /* spiltrae                                                      */
    alfa = -32664;
    beta = 32664;
    free_nodes(root);
    make_node(&root);
  }

  treeon = TRUE;
  thinking = TRUE;
	old_back = backthink;
	if (!(calc || b1calc) || bcalc) 
			backthink = FALSE;
//	show_bt(backthink);
  if (list.nmoves == 1 && !calclib) {
    /* hvis der kun er et muligt treak, saa behoeves vi ikke foretage */
    /* en soegning, men kan fortsaette med det samme                  */
    calc = FALSE;
    bcalc = FALSE;
    moremoves = FALSE;
    value = -32767;
    sofar = 0;
    bestmove = list.move[0];
  }
  else  {
    /* vi har flere treak */

    moremoves = TRUE;
    if (libon || tryagain < 3) {
      /* hvis vi stadig er i biblioteket, og det er sat til */

      if (getlib(computer,&bestmove,playnm,&list,&value)) {
        /* hvis computerens svar findes i biblioteket */
				tryagain = 0;

        search_time = clock();

        search_time -= gam_tid;
        timeleft[computer] -= search_time;
        sofar = 0;
        calc = FALSE;
        bcalc = FALSE;
        search_type = NORMAL_SEARCH;
				make_try(bestmove,0);
				make_res(value, search_time);
				backthink = old_back;
				timeEndPeriod(wTimerRes);
        return bestmove;
      }
			else
				tryagain++;

    }
    delres(&comres[0][0]);
    delres(&humres[0][0]);
    /* initialiser respons killer vaerdierne */

    if (tid_kontrol == spil_tid)
      calc_time();
    /* hvis vi spiler efter en afmaalt tid, saa beregn hvor meget af */
    /* denne tid vi kan bruge paa dette treak                        */

    tid_udlobet = FALSE;

    if (tid_kontrol == tid_per_trek || tid_kontrol == spil_tid) {
      /* hvis soegetypen er tid, saa installer en alarm til at afbryde */
      /* hvis tiden overskrides for meget                              */
			if (!backthink) {
				/*	if (tid_kontrol == tid_per_trek)
					ualarm(10000*rtider[lookahead], 0);
				else if (tid_kontrol == spil_tid)
					ualarm(10000*timemove, 0); */
			}
		//	sigvec(SIGTSTP, &tidskontrol, (struct sigvec *) 0);
		//	sigvec(SIGALRM, &tidskontrol, (struct sigvec *) 0);

				if (tid_kontrol == tid_per_trek)
					msInterval = rtider[lookahead];
				else if (tid_kontrol == spil_tid)
					msInterval = timemove;

				if (MYTimerID != 0) 
						timeKillEvent(MYTimerID);

			  MYTimerID = timeSetEvent(msInterval, // delay
        wTimerRes,                     // resolution (global variable)
        OneShotTimer,               // callback function
        0,                  // user data
        TIME_ONESHOT );                // single timer event
			
      if (setjmp(env)) {
        /* vi retunerer her fra vores soegealgoritme. At vi har lavet det */
        /* paa denne maade med et spring direkte fra soegealgoritmen, og  */
        /* hertil, skyldes at vi saa er sikker paa konsistensen i vores   */
        /* gemte spiltrae. Vi er nu sikker paa at ingen af knuderne peger */
        /* ud i det blaa                                                  */
 
        search_time = clock();
        search_time -= gam_tid;
        timeleft[computer] -= search_time;
        thinking = FALSE;
				sofar--;
				if (bcalc || b1calc) {
					make_try(bestmove,allway + 1);
					make_res(bvalue,search_time);
				}
				else {
					make_try(bestmove, varlook + 1);
					make_res(value,search_time);
				}
				backthink = old_back; 
			//	ualarm(0, 0);
				timeEndPeriod(wTimerRes);
        return bestmove;
      }
    }
    calc = FALSE;
    do {
      if ((allway - varlook) > 7) {
        /* hvis vi er laengere end 7 treak fra sidste niveau i spiltraet */
        /* saa fortsaet med en normal soegning                           */

        sofar = 0;
        search_type = NORMAL_SEARCH;
				hash_get_height = varlook - 1;
				hash_put_height = varlook;
		/*		hash_get_height = min(hash_get_height,10);
				hash_put_height = min(hash_put_height,10); */
				
				
        if (root->barn != NULL) {
          /* hvis der findes noget spiltrae fra en tidligere soegning */
          /* saa brug dette                                           */
          if (varlook > 2 && list.nmoves > 1)
            value = zero_findmax(computer,varlook,0,&mainboard,alfa,beta,&bestmove,&root->barn);
          else
            value = findmax(computer,varlook,0,&mainboard,alfa,beta,&bestmove,&root->barn);
				}
        else
          value = findmax1(computer,varlook,0,0,&list,&mainboard,alfa,beta,&bestmove,&root->barn);

        if (value > 32600)
          /* hvis vi finder en vaerdi der er stoerre end 32600, saa ved */
          /* vi, at computeren har fundet en treakraekkefoelge der kan  */
          /* afslutte spillet. Vi ved saa, at ved en soegning et niveau */
          /* dybere, da vil vaerdien mindst vaere lige saa stor som den */
          /* gamle.                                                     */
          alfa = value - 1;
        else
          alfa = -32664;
        beta = 32664;
        calc = TRUE;
        /* hvis vi naar her saa har vi foretaget en soegning */

        bcalc = FALSE;
        oldlook = varlook;
        /* gem dybden af den sidste soegning */

        varlook++;
        /* soeg et niveau dybere */
      }
      else {
        /* vi er nu saa taet ved slutningen, at vi kan bruge vores slut- */
        /* spils algoritme.                                              */

        if (tid_kontrol == tid_per_trek || tid_kontrol == spil_tid) {
          /* hvis vi bruger en bestemt tid for et helt spil, saa skal vi */
          /* beregne hvor meget tid vi maa bruge paa dette treak         */
          calc_end_time();
          tid_udlobet = FALSE;
					if (!backthink) {
					/*	if (tid_kontrol == tid_per_trek)
							ualarm(10000*rtider[lookahead], 0);
						else if (tid_kontrol == spil_tid)
							ualarm(10000*timemove, 0); */
					}
				//	sigvec(SIGTSTP, &tidskontrol, (struct sigvec *) 0);
				//	sigvec(SIGALRM, &tidskontrol, (struct sigvec *) 0);

					if (tid_kontrol == tid_per_trek)
						msInterval = rtider[lookahead];
					else if (tid_kontrol == spil_tid)
						msInterval = timemove;

					if (MYTimerID != 0) 
						timeKillEvent(MYTimerID);  
			
					MYTimerID = timeSetEvent(msInterval, // delay
					wTimerRes,                     // resolution (global variable)
					OneShotTimer,               // callback function
					0,                  // user data
					TIME_ONESHOT );                // single timer event

          if (setjmp(env)) {
            /* vi retunerer her fra vores soegealgoritme, at vi har lavet det */
            /* paa denne maade med et spring direkte fra soegealgoritmen, og  */
            /* hertil, skyldes at vi saa er sikker paa konsistensen i vores   */
            /* gemte spiltrae. Vi er nu sikker paa at ingen af knuderne peger */
            /* ud i det blaa                                                  */

            search_time = clock();
            search_time -= gam_tid;
            timeleft[computer] -= search_time;
            thinking = FALSE;
						sofar--;
            if (b1calc && !bcalc && bvalue >= 0) {
              /* slutspils soegning med et snaevert alfa-beta vindue, siger */
              /* ikke noget om hvor godt treakket er, hvis vaerdien er      */
              /* mindre end 0, saa vi retunerer det bedste treak ud fra den */
              /* normale alfa-beta soegning                                 */
							if (bcalc || b1calc) {
								make_try(currentbest,allway + 1);
								make_res(bvalue,search_time);
							}
							else {
								make_try(currentbest, varlook);
								make_res(value,search_time);
							}
							backthink = old_back; 
				//			ualarm(0, 0);
              return currentbest;
						}
            else {
							if (bcalc || b1calc) {
								make_try(bestmove,allway + 1);
								make_res(bvalue,search_time);
							}
							else {
								make_try(bestmove, varlook);
								make_res(value,search_time);
							}
							backthink = old_back; 
					//		ualarm(0, 0);
							timeEndPeriod(wTimerRes);
              return bestmove;
						}
          }
        }

        temp = mainboard;
				calc_pos_mov(&temp);
				end_hash = allway - 5;

        alfa = -1;
        beta = 1;
        /* vi soeger foerst med alfa = -1 og beta = 1, for blot at finde */
        /* et treak der vinder. Det er langt hurtigere at soege med et   */
        /* saa snaevert vindue                                           */

        sofar = 0;
        bvalue = 0;
        low_min = TRUE;
        currentbest = bestmove;
				search_type = EXTENTED_SEARCH;

        if (root->barn != NULL)
          /* hvis vi har noget spiltrae fra en tidligere soegning, saa */
          /* brug dette                                                */
          bvalue = (short int)slutmax(computer,allway,0,&temp,(char)alfa,(char)beta,&currentbest,&root->barn);
        else {
          /* vi har intet spiltrae gemt */
          sofar = 1;
					if (mainboard.possible.nmoves < TRESH)
						bvalue = (int)slutmax2(&temp,(char)alfa,(char)beta,&currentbest);
					else
						/* bvalue = (int)slutmax1(computer,allway,0,&list,&temp,alfa,beta,&currentbest,0); */
						bvalue = (int)slutmax1(computer,allway,0,&temp,(char)alfa,(char)beta,&currentbest,0);
						sofar = list.nmoves;
        }
				b1calc = TRUE;
        if (bvalue >= 0)
          bestmove = currentbest;
        low_min = FALSE;
        temp = mainboard;
				calc_pos_mov(&temp);
        if (bvalue >= 1) {
          /* hvis bvalue >= 1, saa har vi fundet en treakraekkefolge der */
          /* vinder, med et bestemt treak. Vi skal nu, hvis der er tid,  */
          /* undersoege om det ogsaa er det treak der vinder med flest   */
          /* brikker. For at undersoege dette skal vi foretage en ny     */
          /* soegning med et storre vindue, og der er her flere optime-  */
          /* ringer vi teoretisk kan foretage.                           */

          if (root->barn != NULL)
            /* hvis vores trae ikke er tomt, saa ved vi at den treak-    */
            /* raekkefolge der foerte til vores vaerdi "bvalue" staar    */
            /* forrest i dette trae. Vi kan altsaa uden risiko for fejl  */
            /* saette alfa = bvalue, og blot soege efter et treak der    */
            /* er bedre. Hvis det saa skulle ske, at det bedste treak,   */
            /* var det foerste, og det gav scoren "bvalue", altsaa en    */
            /* score der ikke var bedre end foer, saa goer det ikke      */
            /* noget, da denne treakraekkefolge alligevel staar forest   */
            alfa = bvalue;
          else
            /* naar vi ikke har noget trae, saa ved vi ikke hvilken      */
            /* raekkefoelge traekkene bliver undersoege i, og vi ved saa */
            /* ikke om det bedste treak bliver undersoege foerst. Vi kan */
            /* saa kun saette alfa til bvalue - 1 daa vi skal vaere      */
            /* sikker paa at dette treak, hvis det er det bedste og hvis */
            /* vaerdien for dette treak er netop bvalue, bliver retune-  */
            /* ret som det bedste                                        */
            alfa = bvalue - 1;

          beta = 64;
          /* vi kan ikke sige noget om hvor godt det kan blive, kun at   */
          /* det ikke kan bliver bedre end 64. (mange programmer bruger  */
          /* slet ikke denne antagelse. Det er egentlig overraskende, da */
          /* de saa hvis de har fundet en treakraekkefolge der foerer    */
          /* til en vundet stilling med 64 treak, alligevel fortsaetter  */
          /* med at soege, selv om det er fuldstaendigt udnoedvendigt.   */
          /* Endvidre er denne "optimering" helt gratis.                 */

          if ((sofar > 1) && (root->barn != NULL))
            /* sofar er en variabel, der holder rede paa hvor mange      */
            /* treak der blev undersoegt paa laveste niveau i sidste     */
            /* soegning. Hvis vores trae nu ikke er tomt, og vi har      */
            /* undersoegt mere end et treak, sa ved vi her, at de fore-  */
            /* gaaende treak alle var treak der ikke foerte til en       */
            /* stilling. Der er derfor ingen grund til at undersoege dem */
            /* naar vi skal finde det treak der vinder mest. Dette er    */
            /* en optimering jeg ikke har set andre steder foer.         */
            do {
              trtemp = root->barn->sosk;
              root->barn->sosk = root->barn->sosk->sosk;
              trtemp->sosk = NULL;
              free_nodes(trtemp);
            }
            while (--sofar > 1);
        } else if (bvalue <= -1) {
          /* hvis bvalue <= -1, saa er spillet tabt */
          alfa = -64;

          beta = bvalue;
          /* der er ingen grund til at soege efter noget der er bedre */
          /* end bvalue                                               */

        } else {
          /* scoren var 0, altsaa er den indenfor det snaevre alfa-beta  */
          /* vindue, hvilket vil sige at det er en noejagtig vaerdi. Vi  */
          /* behoeves saa ikke at soege mere                             */
          bcalc = TRUE;
          calc = FALSE;
          break;
        }
        if (stop_nu(0))
          /* undersoeg, om der er tid til at foretage en ekstra soegning */
          break;
        sofar = 0;
        if (root->barn != NULL && list.nmoves > 1)
          /* hvis spiltraeet ikke er tomt, saa brug dette */
          bvalue = (short int)zero_slutmax(computer,allway,0,&temp,alfa,beta,&bestmove,&root->barn);
        else{
          sofar = 1;
					if (mainboard.possible.nmoves < TRESH)
						bvalue = (int)slutmax2(&temp,alfa,beta,&bestmove);
					else
					/*	bvalue = (int)slutmax1(computer,allway,0,&list,&temp,alfa,beta,&bestmove,0); */
						bvalue = (int)slutmax1(computer,allway,0,&temp,alfa,beta,&bestmove,0);
						sofar = list.nmoves;
        }
        bcalc = TRUE;
        calc = FALSE;
				break;
        /* vi er faerdige  */
      }
    }
    while (!stop_nu(varlook));
    /* indtil ikke mere tid, eller max soegedybde er naaet */
  }
  
	if (backthink) {
		search_time = 0;
	}
	else {

    search_time = clock();
		search_time -= gam_tid;
		timeleft[computer] -= search_time;
		/* opdater den resterende tid */
	}

  thinking = FALSE;
	sofar--;
	
	if (bcalc || b1calc) {
		make_try(bestmove,allway + 1);
		make_res(bvalue,search_time);
	}
	else {
		make_try(bestmove, varlook);
		make_res(value,search_time);
	}


	backthink = old_back; 
//	ualarm(0, 0);
	timeEndPeriod(wTimerRes);
  return bestmove;
  /* retuner det bedste treak */
}
