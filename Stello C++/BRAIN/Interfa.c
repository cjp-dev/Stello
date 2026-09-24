/* #include <curses.h>	 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
/* #include <fcntl.h> */
#include <ctype.h>
#include <signal.h>
#include "reversi.h"
#include "hash.h"
#include "book.h"

#define StrCopy(s, d)	while ((*d++ = *s++) != 0)
/* #define min(a, b)		((a) < (b) ? (a) : (b))
#define max(a, b)		((a) > (b) ? (a) : (b)) */


typedef struct
		{ char name[30];
			char	Document[80][30];
			short int	DocTop;			/* First line in document  */
			short int	Height;			/* Size of window in chars */
			short int	Lines;			/* Length of doc in lines  */
			short int	WindId;
			short int	isopen;
		} GAME;
		
typedef struct
		{	char name[30];
			short int	WindId;
			short int	isopen;
		} LILLE;
		
typedef struct
		{	short int timedef,leveldyb,leveltid;
			short int 	borderdef;
			short int   hvmudef;
			short int   notadef;
			short int   anadef;
			short int   gamdef;
		} config;
		
									
GAME Text;
LILLE Ana,Rev;
config revdef;

#define column(s) (s%10 - 1)
#define row(s) (s/10 - 1)

#define TAB (43)
/* coordinates within a square for the following are ([1,5],[1,3]) */
#define XBOR 5
#define YBOR 5
#define SQW (4)
#define SQH (2)
#define Vcoord(s,x,y) \
	(SQW*column(s) + x),(SQH*(row(s)) + y)


short int border_on,lastdyb,lasttid,lastspil,backthink;
short int frem,tilbage,anaopen,gamopen,gamfirst,anafirst, posible,calclib;
short int hvmul,play,pass,orient,treknu,markoer, tryagain;
volatile short int curcl,clon,nytid,clhvid,clsort;

char nmoves[30];        /* forventede trek */
char nmoves1[30];
gamerec game;
savegame sgame;

char st_tid[15][20] =
{" Sek  0.1  ",
 " Sek  0.25 ",
 " Sek  0.5  ",
 " Sek  1    ",
 " Sek  2    ",
 " Sek  5    ",
 " Sek 10    ",
 " Sek 20    ",
 " Sek 30    ",
 " Sek 45    ",
 " Min  1    ",
 " Min  2    ",
 " Min  3    ",
 " Min  5    ",
 " Evigt     " };
 
char st_dybde[15][20] =
{" Sogedybde  1 ",
 " Sogedybde  2 ",
 " Sogedybde  3 ",
 " Sogedybde  4 ",
 " Sogedybde  5 ",
 " Sogedybde  6 ",
 " Sogedybde  7 ",
 " Sogedybde  8 ",
 " Sogedybde  9 ",
 " Sogedybde 10 ",
 " Sogedybde 11 ",
 " Sogedybde 12 ",
 " Sogedybde 13 ",
 " Sogedybde 14 ",
 " Sogedybde 15 " };
 
char st_game[15][20] =
{"   1 sek game ",
 "   2 sek game ",
 "   5 sek game ",
 "  10 sek game ",
 "  20 sek game ",
 "  30 sek game ",
 "  45 sek game ",
 "   1 min game ",
 "   2 min game ",
 "   5 min game ",
 " 7.5 min game ",
 "  10 min game ",
 "  15 min game ",
 "  20 min game ",
 "  30 min game " };

 
int calculating;

#ifdef DEBUG 
FILE * debugf;
#endif

#define MARKOER_OFF 0

void
gotoXY (short int x, short int y)
{
  move (y - 1, x - 1);
}

void
OReverse (void)
{
  standout ();
}

void
ONormal (void)
{
  standend ();
}

short int alert(char * s,char p1,char p2) 
{
	short int tast;
	
	if (calclib)
		return 0;
		
	flushinp();
	
	gotoXY (TAB, 8);

	OReverse ();
  printw(s);
  refresh();

  ONormal ();
	do {
		/* putchar(7); */
		tast = getch ();
	}
	while (tast != p1 && tast != p2 && tast != 13);
	if (tast == 13)
		tast = p1;

	gotoXY (TAB, 8);
	clrtoeol ();
	refresh();
	if (tast == p1)
		return 0;
	return 1;
}

void get_string(char * d, char * s) 
{	
	char test;
	flushinp();
	gotoXY (TAB, 8);

  printw(s);
  refresh();
  
	nl();
	echo(); 

	scanw("%s", d); 
  cbreak();
	nonl();
	noecho();
	
	gotoXY (TAB, 8);
	clrtoeol ();
	refresh();
}


void setsquare(short int sq,short int c)
{      
	if (calclib)
		return;
  flushinp();
  gotoXY (Vcoord (sq,XBOR + 1,YBOR + 1));

  switch (c)
    {
    case DARK:
			OReverse ();
      printw (" * ");
      ONormal ();
      break;
    case EMPTY:
			printw ("   ");
			ONormal ();
      break;
    case LIGHT:
      printw (" o ");
      break;
    case MARKER:
      printw (" * ");
      break;
    }
}
      
void
ShowPlayers (void)
{
		if (calclib)
		return;
	flushinp();
  gotoXY (5, 2);
  printw ("%s", (computer == DARK) ? "Computer er sort" : "Menneske er sort");
  gotoXY (5, 3);
  printw ("%s", (computer == LIGHT) ? "Computer er hvid" : "Menneske er hvid");
}


void
ShowHeader (void)
{
		if (calclib)
		return;
	flushinp();
  gotoXY (TAB, 2);
  printw ("C.J.P Reversi");
  gotoXY (TAB, 3);
  if (tid_kontrol == sogedybde)
    printw ("Svaerhedsgrad = %s",st_dybde[lookahead]);
  else if (tid_kontrol == tid_per_trek)
    printw ("Svaerhedsgrad = %s",st_tid[lookahead]);
	else
	  printw ("Svaerhedsgrad = %s",st_game[lookahead]);
  clrtoeol ();
	gotoXY (TAB, 4);
	if (backthink)
		printw ("think on opp = ON ");
	else
		printw ("think on opp = OFF");
	gotoXY (TAB, 5);
	if (tid_kontrol == spil_tid) {
		printw ("Rest tid = %d",timeleft[computer]);
		clrtoeol ();
	}
	else
		clrtoeol ();

}

void
show_bt (short int bt)
{
		if (calclib)
		return;
	flushinp();
	if (bt) {
		gotoXY (TAB, 6);
		printw ("backthinking = YES ");
	}
	else {
		gotoXY (TAB, 6);
		printw ("backthinking = NO ");
	}
	refresh ();
}


#define HEADER 1
#define PLAYERS 2
#define BRIKS 4
#define BOARD 8
#define ALL BOARD|BRIKS|PLAYERS|HEADER

void
UpdateDisplay (short int redraw)
{
  short i,j, z;
  	if (calclib)
		return;
	flushinp();
  if (redraw & HEADER)
    ShowHeader ();
  if (redraw & PLAYERS)
    ShowPlayers ();
  if (redraw & BOARD) {  
    i = 0;
    gotoXY (XBOR, YBOR + i++);
    printw ("+---+---+---+---+---+---+---+---+");
    while (i < 16) {
			gotoXY (XBOR - 2,YBOR + i++);
			z = (i / 2);
			printw ("%d |   |   |   |   |   |   |   |   |", z);
			gotoXY (XBOR,YBOR + i++);
			if (i < 16)
			  printw ("+---+---+---+---+---+---+---+---+");
		}
    printw ("+---+---+---+---+---+---+---+---+");
    gotoXY (XBOR,YBOR + i);
	  printw ("  a   b   c   d   e   f   g   h");
  }
   
  if (redraw & BRIKS) {
    for (i = 1; i <= 8; i++)
      for (j = 1; j <= 8; j++) 
        setsquare(10*i + j, mainboard.sq[10*i + j]); 
    if (markoer != MARKOER_OFF)
	    setsquare(markoer,MARKER);
  }

  gotoXY (1,1);
  refresh ();
}



void text_move(short int move, char *tx)
{	char temp[4];
	
	strcpy(temp,"   ");                          
	if (move == 0) {
		temp[0] = '-';
		temp[1] = '-';
	} 
	else {
		temp[0] = 'A' - 1 + (move % 10);
		if (orient == 0)
			temp[1] = '0' + (move / 10);
		else
			temp[1] = '0' + 9 - (move / 10);
	}
	strcpy(tx,temp);
}

void itoa(int val,char *d)
{
	char s[80];
 	int x,y;
	
	x = 0;
	do
		s[x++] = val % 10 + '0';
	while (val /= 10);
	d[x] = 0;
	y = 0;
	while (--x >= 0)
		d[y++] = s[x];
}

void make_try(short int move,short int look)
{  char temp[10];
	if (calclib)
		return;

  flushinp();

	text_move(move, temp);
  gotoXY (TAB, 14);
  printw ("Treak = %sDybde = %d, %d/%d   ", temp, look, sofar + 1,posible);
	refresh ();
}

void make_res(short int value, long tid)
{	char temp[10];
	int x,hasp;
	tree *ntree;
		if (calclib)
		return;

	flushinp();
	
	gotoXY (TAB, 10);
	if (bcalc)
  	printw ("Braedtvaerdi = %d p    ",value);
	else if(b1calc)
		printw ("Braedtvaerdi = %d e    ",value);
	else
	  printw ("Braedtvaerdi = %d      ",value);
	gotoXY (TAB, 11);
  printw ("Knuder = %ld      ",aknud);

	gotoXY (TAB, 12);
  printw ("Evalueringer = %ld       ",aeval);
 
 	gotoXY (TAB, 13);
  printw ("Tid = %ld       ",tid);

	gotoXY (TAB, 15);
	strcpy(nmoves,"Traek = ");
	x = 0;
	ntree = root->barn;
	while (x++ <= 5 && ntree != NULL) {
		text_move(ntree->move, temp);
		strcat(nmoves,temp);
		ntree = ntree->barn;
	}
  printw ("%s              ",nmoves);
	gotoXY (TAB, 16);
	strcpy(nmoves1,"        ");
	x = 0;
	while (x++ <= 5 && ntree != NULL) {
		text_move(ntree->move, temp);
		strcat(nmoves1,temp);
		ntree = ntree->barn;
	}
	printw ("%s              ",nmoves1);

	gotoXY (TAB, 17);
  printw ("Allokeringer = %ld      ",aalloc);

	gotoXY (TAB, 18);
  printw ("Hashcall = %d     ", HashCall);
	gotoXY (TAB, 19);
	if (HashCall != 0)
	  hasp = (HashHit*100)/HashCall;
	else
	  hasp = 0;
  printw ("Hashhit  = %d %%%d    ", HashHit,hasp);
	gotoXY (TAB, 20);
	if (HashCall != 0)
	  hasp = (TransHit*100)/HashCall;
	else
	  hasp = 0;
  printw ("Transhit = %d %%%d    ", TransHit,hasp);

	refresh ();
}


/**********************************************/
/*              FILE OPERATIONS               */
/**********************************************/

void move_to_text(short int mvnm,short int move, char *tx)
{	char temp[30];
	
	strcpy(temp,"                    ");                          
	if (mvnm % 2) {
		if (mvnm < 9) 
			temp[2] = '0' + mvnm + 1;
		else {
			temp[1] = '0' + ((mvnm+1) / 10);
			temp[2] = '0' + ((mvnm+1) % 10);
		}
		if (move == 0) {
			temp[16] = 'P';
			temp[17] = 'A';
			temp[18] = 'S';
			temp[19] = 'S';
		} 
		else {
			temp[17] = 'A' - 1 + (move % 10);
			if (orient == 0)
				temp[18] = '0' + (move / 10);
			else
				temp[18] = '0' + 9 - (move / 10);
		}
		strcpy(tx,temp);
	}
	else {
		if (mvnm < 9) 
			temp[2] = '0' + mvnm + 1;
		else {
			temp[1] = '0' + ((mvnm+1) / 10);
			temp[2] = '0' + ((mvnm+1) % 10);
		}
		if (move == 0) {
			temp[6] = 'P';
			temp[7] = 'A';
			temp[8] = 'S';
			temp[9] = 'S';
		} 
		else {
			temp[7] = 'A' - 1 + (move % 10);
			if (orient == 0)
				temp[8] = '0' + (move / 10);
			else
				temp[8] = '0' + 9 - (move / 10);
		}
		strcpy(tx,temp);
	}
}
	
void update_move(short int mvnm)
{	
	move_to_text(mvnm - 1,game.moves[mvnm - 1],Text.Document[mvnm - 1]);
	Text.Lines  = mvnm;
	Text.DocTop = mvnm;
}

void game_to_text(short int lines)
{	
	int x;

	for (x = 0; x < lines; x++)
		move_to_text(x,game.moves[x],Text.Document[x]);
	Text.Lines  = lines;
	Text.DocTop = lines;
} 

void merge_game(void)
{
	short int sum;
	
	if (alert("Spil i database ? (j,n)",'j','n') == 0) {
		convert_game();
			if (playnm > 0) {
				tilbage = TRUE;
			} 
			else {
				tilbage = FALSE;
			}
			frem = FALSE;
	  	curcl = human;
	    sum = makelist(&list, computer, &mainboard) +
						makelist(&list, human, &mainboard);
			if (sum == 0) {
				gameover = TRUE;
        clon = FALSE;
      }
      else {
      	gameover = FALSE;
        clon = TRUE;
      }

		if (makelist(&list, human, &mainboard))
			markoer = list.move[0];
		else
			markoer = MARKOER_OFF;
		UpdateDisplay(ALL);
		
		if (alert("Hvem vandt ? (s,h)",'s','h') == 0)
			mmgame(1,-32665, &book_root); 
		else
			mmgame(1,32665, &book_root);
	}
}

void load_game(void)
{
 char GamePath[256]="";
 FILE * fd;
 int sum,player,x;
 
	get_string(GamePath,"Hent spil : ");
	
	 	if ((fd = fopen(GamePath, "r")) != NULL) {
			if (fread(&sgame, sizeof(sgame),1,fd) == 1) {
		  fclose(fd); 

			calc = FALSE;
			bcalc = FALSE;
			init_game();
			playnm = game.sidste = sgame.sidste;
			player = DARK;
			for (x = 0; x <= playnm; x++) {
				game.moves[x] = sgame.moves[x];
				game.boards[x] = mainboard;
				if (sgame.moves[x] != 0)
					trymove(sgame.moves[x],player,&mainboard);
				player = other(player);
			}
			game_to_text(game.sidste);
			mainboard = game.boards[playnm];
			if (playnm % 2)
				human = LIGHT;
			else
				human = DARK;
			computer = other(human);
			if (playnm > 0) {
				/* menu_ienable(M_tree,SPBACK,TRUE); */ 
				tilbage = TRUE;
			} 
			else {
				/* menu_ienable(M_tree,SPBACK,FALSE); */
				tilbage = FALSE;
			}
			/* menu_ienable(M_tree,SPFREM,FALSE); */
			frem = FALSE;
	  	curcl = human;
	 /* 	if (human == DARK) {
	  		strcpy(Text.name," Spiller    Atari  ");
	  		wind_set(Text.WindId, WF_NAME, Text.name);
	  	}
	  	else {
	  		strcpy(Text.name,"  Atari    Spiller ");
	  		wind_set(Text.WindId, WF_NAME, Text.name);
	  	} */
	   /* reset_clock(); */
	    sum = makelist(&list, computer, &mainboard) +
						makelist(&list, human, &mainboard);
			if (sum == 0) {
				gameover = TRUE;
      /*  menu_ienable(M_tree,SPSKSI,FALSE); */
        clon = FALSE;
      }
      else {
      	gameover = FALSE;
     /*   menu_ienable(M_tree,SPSKSI,TRUE); */
        clon = TRUE;
      }
      if (makelist(&list, human, &mainboard))
      	markoer = list.move[0];
      else
      	markoer = MARKOER_OFF;
      UpdateDisplay(ALL);
			}
			else {
				fclose(fd);
				alert("Forkert filformat (Ok)",'o',0);
			}
		}
		else
			alert("Fil findes ikke (Ok)",'o',0);
} 

void save_game(void) 
{
	char GamePath[256]="";
	FILE * fd;
  int x;
  
	get_string(GamePath,"Gem spil : ");
		if ((fd = fopen(GamePath, "w")) != NULL) {
			for (x = 0; x <= 79; x++)
				sgame.moves[x] = 0;
			sgame.sidste = game.sidste;
			for (x = 0; x <= game.sidste; x++)
				sgame.moves[x] = game.moves[x];
			 fwrite(&sgame, sizeof(sgame),1,fd); 
			fclose(fd);
		} 
}

/**********************************************/
/*              FAA COMPUTER TREAK            */
/**********************************************/


void get_com(void)
{
	int trek;
  char str[40],mess[40];
	
  if (makelist(&list, computer, &mainboard)) {
    trek = getcomputer();
    /* putchar(0x07); */
    makemove(trek, computer);
		text_move(trek,str);
		strcpy(mess,"Jeg trak ");
		strcat(mess, str);
		strcat(mess, ". (Ok)");
		alert(mess,'o',0);
    if ((makelist(&list, computer, &mainboard) +
    		makelist(&list, human, &mainboard)) == 0)
    	gameover = TRUE;
  }
  else { /* computer kan ikke trkke */
		if (makelist(&list, human, &mainboard)) {
			alert("Jeg kan ikke trekke. (Ok)",'o',0);
			
			calc = FALSE;
			bcalc = FALSE;
			game.moves[playnm++] = 0;
			game.sidste = playnm;
			game.boards[playnm] = mainboard;
			update_move(playnm);
    }
    else
    	gameover = TRUE;
  }
}

void game_logic(void)
{
  get_com();
	ShowHeader ();
  calculating = FALSE;
} 

/**********************************************/
/*              MENU OPERATIONS               */
/**********************************************/

void declarewinner(void)
{
  int diff;
  char s[80],d[80];
		if (calclib)
		return;


  diff = mainboard.ndiscs[computer] - mainboard.ndiscs[human];
  if (diff > 0) {
    itoa(diff,s);
    strcpy(d,"Jeg vandt med ");
    strcat(d, s);
    strcat(d, " (Ok) ");
    alert(d,'o',0);
  }
  else if (diff < 0) {
		itoa(-diff,s);
    strcpy(d,"Du vandt med ");
    strcat(d, s);
    strcat(d, " (Ok) ");
    alert(d,'o',0);
  }
  else
  	alert("  Remis! (Ok) ",'o',0);
}

void newspil(void)
{
  if (alert("Starte paa nyt spil. (j,n)",'j','n') == 0) {
  	/* menu_ienable(M_tree,SPSKSI,TRUE); */
    init_game();
		if (libok) {
      libon = TRUE;
			tryagain = 0;
		}
    /* menu_ienable(M_tree,SPBACK,FALSE); */
		tilbage = FALSE;
		/* menu_ienable(M_tree,SPFREM,FALSE); */
		frem = FALSE;
		calc = FALSE;
		bcalc = FALSE;
  	curcl = DARK;
  	Text.Lines  = 0;
		Text.DocTop = 0;
		clon = FALSE;
		/* reset_clock(); */
		markoer = list.move[0];
		UpdateDisplay(ALL);
    clon = TRUE;
  }
}


int Abandon(void)
{	
	if (alert("Stop med spil ? (j,n)",'j','n') == 0)
		return TRUE;
	return FALSE;
}

void thinkopp(void)
{	
	if (alert("Taenk i modstander tid ? (j,n)",'j','n') == 0)
		backthink = TRUE;
	else
		backthink = FALSE;
	ShowHeader ();
}


 
#define DIASOG 6

void dybde(void)
{
	int menstart,x,tast;

	do {
			menstart = DIASOG;
		  for (x = 0; x <= 14; x++) {
		    gotoXY (TAB, menstart++);
		    if (x == lastdyb)
			    OReverse ();
	      printw ("%s",st_dybde[x]);
	      if (x == lastdyb)
		      ONormal ();
	    }
	    gotoXY (1, 1);
	    refresh();
		  tast = getch ();
		  if (tast == 'w' || tast == '8')
		    if (lastdyb > 0)
			    lastdyb--;
		    else
			    putchar(7);
		  if (tast == 'x' || tast == '2')
		    if (lastdyb < 14)
			    lastdyb ++;
		    else
			    putchar(7);
	
		}
		while (tast != 13);

		menstart = DIASOG;
		for (x = 0; x <= 14; x++) {
		  gotoXY (TAB, menstart++);
		  clrtoeol ();
	  }
		gotoXY(1,1);
		refresh();

  tid_kontrol = sogedybde;
  /* menu_icheck(M_tree,SOGE,TRUE); 
  menu_icheck(M_tree,TIDPTK,FALSE); */
  lookahead = lastdyb;
  calc = FALSE;
	bcalc = FALSE;
	ShowHeader ();
	refresh();
}

void tid_p_trek(void)
{
	int menstart,x,tast;
	
		do {
			menstart = DIASOG;
		  for (x = 0; x <= 14; x++) {
		    gotoXY (TAB, menstart++);
		    if (x == lasttid)
			    OReverse ();
	      printw ("%s",st_tid[x]);
	      if (x == lasttid)
		      ONormal ();
	    }
	    gotoXY (1, 1);
	    refresh();
		  tast = getch ();
		  if (tast == 'w' || tast == '8')
		    if ( lasttid> 0)
			    lasttid--;
		    else
			    putchar(7);
		  if (tast == 'x' || tast == '2')
		    if (lasttid < 14)
			    lasttid++;
		    else
			    putchar(7);
	
		}
		while (tast != 13);

		menstart = DIASOG;
		for (x = 0; x <= 14; x++) {
		    gotoXY (TAB, menstart++);
		    clrtoeol ();
	    }
		gotoXY(1,1);
		refresh();


   tid_kontrol = tid_per_trek;
  /* menu_icheck(M_tree,SOGE,FALSE); 
  menu_icheck(M_tree,TIDPTK,TRUE); */
  lookahead = lasttid;
  calc = FALSE;
	bcalc = FALSE;
	ShowHeader ();
	refresh();
}

void tid_p_game(void)
{
	int menstart,x,tast;
	
		do {
			menstart = DIASOG;
		  for (x = 0; x <= 14; x++) {
		    gotoXY (TAB, menstart++);
		    if (x == lastspil)
			    OReverse ();
	      printw ("%s", st_game[x]);
	      if (x == lastspil)
		      ONormal ();
	    }
	    gotoXY (1, 1);
	    refresh();
		  tast = getch ();
		  if (tast == 'w' || tast == '8')
		    if ( lastspil> 0)
			    lastspil--;
		    else
			    putchar(7);
		  if (tast == 'x' || tast == '2')
		    if (lastspil < 14)
			    lastspil++;
		    else
			    putchar(7);
	
		}
		while (tast != 13);

		menstart = DIASOG;
		for (x = 0; x <= 14; x++) {
		    gotoXY (TAB, menstart++);
		    clrtoeol ();
	    }
		gotoXY(1,1);
		refresh();


  tid_kontrol = spil_tid;
  /* menu_icheck(M_tree,SOGE,FALSE); 
  menu_icheck(M_tree,TIDPTK,TRUE); */
  lookahead = lastspil;
	timeleft[0] = spil_tider[lookahead];
  timeleft[1] = spil_tider[lookahead];
  for (x = 0; x <= playnm; x++)
    timesleft[x] = spil_tider[lookahead];
  calc = bcalc = b1calc = FALSE;
	ShowHeader ();
	refresh();
}


void skiftside(short int * sksid)
{
	calc = FALSE;
	bcalc = FALSE;
  computer = human;
  human = other(human);
  markoer = MARKOER_OFF;
  UpdateDisplay(PLAYERS|BRIKS);
	play = TRUE;
	*sksid = TRUE;
}

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

void trkbage(void)
{
	int y;
		
	if (playnm > 0) {
    playnm--;
		timeleft[computer] = timesleft[playnm];
    Text.Lines--;
    clon = TRUE;
    calc = FALSE;
		bcalc = FALSE;
	/*	clear_hash(); */
		if (libok) {
      libon = TRUE;
			tryagain = 0;
		}
		delres(&comres[0][0]);
		delres(&humres[0][0]); 
    if (playnm == 0) {
    	/* menu_ienable(M_tree,SPBACK,FALSE); */
    	tilbage = FALSE;
    }
    if (!frem) {
  		/* menu_ienable(M_tree,SPFREM,TRUE); */
  		frem = TRUE;
  	}
  /*  if (gameover) 
      menu_ienable(M_tree,SPSKSI,TRUE); */
    if (playnm % 2)
			human = LIGHT;
		else
			human = DARK;
    computer = other(human);
    curcl = human;
    mainboard = game.boards[playnm];
    if (makelist(&list, human, &mainboard))
		  markoer = list.move[0];
		else
		  markoer = MARKOER_OFF;
    gameover = FALSE;
    y = Text.Lines - Text.Height;
		y = max(0, y);
		if (Text.DocTop != y) 
		 	Text.DocTop = y;
    UpdateDisplay(BRIKS|PLAYERS);
		ShowHeader ();
	} 
}     

void trkfrem(void)
{
	int y;
	
  if (playnm < game.sidste) {
    playnm++;
		timeleft[computer] = timesleft[playnm];
    Text.Lines++;
    calc = FALSE;
		bcalc = FALSE;
		delres(&comres[0][0]);
		delres(&humres[0][0]); 
    if (playnm == game.sidste) {
  		/* menu_ienable(M_tree,SPFREM,FALSE); */
  		frem = FALSE;
    }
    if (!tilbage) {
      /* menu_ienable(M_tree,SPBACK,TRUE); */
    	tilbage = TRUE;
  	}
    human = other(playnm % 2);
    computer = other(human);
    curcl = human;
    mainboard = game.boards[playnm];
    if (makelist(&list, computer, &mainboard) + 
    	 makelist(&list, human, &mainboard) == 0) {
        gameover = TRUE;
        /* menu_ienable(M_tree,SPFREM,FALSE); */
		  	frem = FALSE;
		  	clon = FALSE;
        /* menu_ienable(M_tree,SPSKSI,FALSE); */
      }
    if (makelist(&list, human, &mainboard))
		  markoer = list.move[0];
		else
		  markoer = MARKOER_OFF;
		y = Text.Lines - Text.Height;
		y = max(0, y);
		if (Text.DocTop != y) 
		 	Text.DocTop = y;
		UpdateDisplay(BRIKS|PLAYERS);
		ShowHeader ();
	}
}


#define INTET 0
#define MENU 1
#define POINTER_PRESSED 2
#define KEYPRESS 3

#define NORD 1
#define SYD 2
#define WEST 3
#define OEST 4

int check_move(short int retning)
{
	if (markoer != MARKOER_OFF) {
		switch (retning) {
			case NORD:
				if (markoer > 20) {
					markoer -= 10;
					return TRUE;
				}
				else
					return FALSE;
			case SYD:
				if (markoer < 80) {
					markoer += 10;
					return TRUE;
				}
				else
					return FALSE;
			case WEST:
				if (markoer%10 > 1) {
					markoer -= 1;
					return TRUE;
				}
				else
					return FALSE;
			case OEST:
				if (markoer%10 < 8) {
					markoer += 1;
					return TRUE;
				}
				else
					return FALSE;
	  }
	}
	else
		return FALSE;
	return FALSE;
}

#define FORTRYD 0
#define NYTSPI 1
#define HENSPI 2
#define GEMSPI 3
#define FLETSPI 4
#define STOSPI 5
#define SOGDYB 6
#define TIDPTK 7
#define TIDGAM 8
#define SPBACK 9
#define SPFREM 10
#define SPSKSI 11
#define BACKTHINK 12

int Handle_menu(int obj,short int * sksid) 
{
  int done = FALSE;

  switch (obj) {
  /*  case REVINF : disinf(); break; */
    case NYTSPI : newspil(); break; 
    case HENSPI : load_game(); break;
    case GEMSPI : save_game(); break;
		case FLETSPI :merge_game(); break;
    case STOSPI : done = Abandon(); break;
    case SOGDYB : dybde(); break;
    case TIDPTK : tid_p_trek(); break;
		case TIDGAM : tid_p_game(); break;
	/*  case MULTRK : muligonof(); break; */
	  case SPSKSI : skiftside(sksid); break;
/*	  case NULUR  : reset_clock(); break;
	  case GAMWIN : Game_on_of(); break;
	 	case ANAWIN : Ana_on_of(); break;
	 	case VENDB  : vend_br(); break; */
    case SPBACK : trkbage(); break;
    case SPFREM : trkfrem(); break;
		case BACKTHINK : thinkopp(); break;
/*    case TRKNU  : treknu = TRUE; break;
    case TRKPRI : udskriv(); break; 
		case GEMCONF : gem_conf(); break; */
    default: ;
	}
  return done;
}

int Handle_key(short int key,short int * sksid)
{
	int done = FALSE;
		
  switch (key) {
	  case 'S' : skiftside(sksid); break;
    case 'T' : trkbage(); break;
    case 'F' : trkfrem(); break;
    case 'P' : if (makelist(&list, human, &mainboard) == 0) {
    						 play = TRUE;
                 pass = TRUE;
               }
               else
               	putchar(7);
               break;
    case 'Q' : done = Abandon(); break;
    default: ;
	}
	return done;
}
#define MENU_TOP 10
#define MENU_NUM 3

char menu_items[MENU_NUM][10][20] = 
{{"|---------->","  Nyt spil  ","  Hent spil ","  Gem spil  ","  Flet spil ","  Stop   (Q)"},
 {"<------------->","  Sogedybde     ","  Tid pr. traek ","  Tid pr. spil  "},
 {"<-------------|","  Frem      (F)","  Tilbage   (T)","  Skiftside (S)","  Taenk i modst"}
			 };
short int sub_menu[MENU_NUM] =
{5,3,4};
   
short int menu_valg[MENU_NUM][10] =
 { {0,NYTSPI,HENSPI,GEMSPI, FLETSPI,STOSPI},
   {0,SOGDYB,TIDPTK,TIDGAM},
   {0,SPFREM,SPBACK,SPSKSI, BACKTHINK}
													};


void slet_menu(int menu)
{
		int x,menstart = MENU_TOP;
		
		for (x = 0; x <= sub_menu[menu]; x++) {
		    gotoXY (TAB, menstart++);
		    clrtoeol();
	    }
		gotoXY(1,1);
		refresh();
}

short int disp_menu(void)
{
	short int x,tast,top_menu, c_sub,menstart;
														
	top_menu = 0;
	c_sub = 1;
	clear ();
	UpdateDisplay(ALL);
	do {
		do {
			menstart = MENU_TOP;
		  for (x = 0; x <= sub_menu[top_menu]; x++) {
		    gotoXY (TAB, menstart++);
		    if (x == c_sub)
			    OReverse ();
	      printw ("%s",menu_items[top_menu][x]);
	      if (x == c_sub)
		      ONormal ();
	    }
	    gotoXY (1, 1);
	    refresh();
		  tast = getch ();
		  if (tast == 'w' || tast == '8')
		    if (c_sub > 1)
			    c_sub--;
		    else
			    putchar(7);
		  if (tast == 'x' || tast == '2')
		    if (c_sub < sub_menu[top_menu])
			    c_sub++;
		    else
			    putchar(7);
	
		}
		while (tast != 27 && tast != 13 && tast != 'a' && tast != '4' && tast != 'd' && tast != '6');
			if (tast == 'a' || tast == '4')
		    if (top_menu > 0) {
		    	c_sub = 1;
		    	slet_menu(top_menu--);
		    }
		    else
			    putchar(7);
		  if (tast == 'd' || tast == '6')
		    if (top_menu < MENU_NUM - 1) {
		    	c_sub = 1;
			    slet_menu(top_menu++);
			  }
		    else
			    putchar(7);
		}
		while(tast != 13 && tast != 27);
		slet_menu(top_menu);
	if (tast == 27)
		return FORTRYD;
	else
		return (menu_valg[top_menu][c_sub]);
}
#define PASS 0

void get_evnt(short int * evnt, short int * position, short int * menuvalg,
              short int * key)
{
	short int tast;
	
	gotoXY(1,1);
	refresh ();

	*evnt = INTET;
	*menuvalg = INTET;
	*position = INTET;

	
	tast = getch ();
	if (tast == 'w' || tast == '8') {
		if (check_move(NORD)) 
			UpdateDisplay(BRIKS);
		else
			putchar(7);
	} else if (tast == 'a' || tast == '4') {
		if (check_move(WEST)) 
			UpdateDisplay(BRIKS);
		else
			putchar(7);
	} else if (tast == 'x' || tast == '2') {
		if (check_move(SYD)) 
			UpdateDisplay(BRIKS);
		else
			putchar(7);
	} else if (tast == 'd' || tast == '6') {
		if (check_move(OEST)) 
			UpdateDisplay(BRIKS);
		else
			putchar(7);
	} else if (tast == 13) {
	  *evnt = POINTER_PRESSED;
	  *position = markoer;
	} else if (tast == 'm') {
	  if ((*menuvalg = disp_menu()) != FORTRYD)
	  	*evnt = MENU;
	}
	else {
	  *evnt = KEYPRESS;
	  *key = tast;
	}
}

int inlist(short int b, movelist *l)
{    
  short int z;

  for (z = 0; (char)z < l->nmoves; z++)
    if (l->move[z] == (char)b) 
      return TRUE;
  return FALSE;
}

void Spil(void)
{
	short int done = FALSE,sksid = FALSE;
	short int evnt,trek,menuvalg,key;
	
	calculating = FALSE;
	
	do {
	  get_evnt(&evnt, &trek, &menuvalg, &key);
	  if (evnt == POINTER_PRESSED) {
	  	if (!gameover) 
				if (makelist(&list, human, &mainboard)) 
			    if (inlist(trek,&list)) 
			      play = TRUE;
			    else 
			    	putchar(7);
		}	
	  else if (evnt == MENU)
	  	done = Handle_menu(menuvalg,&sksid);
	  else if (evnt == KEYPRESS)
	  	done = Handle_key(key,&sksid);
	  	
	  if (play) {
			if (!tilbage) {
		  	tilbage = TRUE;
			}
		  if (frem) {
				frem = FALSE;
			}
	    if (sksid) 
	    	sksid = FALSE;
 			else {
 				if (pass) {
 					pass = FALSE;
 					if (!makelist(&list, computer, &mainboard)) {
 						makelist(&list, human, &mainboard);
						gameover = TRUE;
					}
					else {
						game.moves[playnm++] = 0;
						game.sidste = playnm;
						game.boards[playnm] = mainboard;
						update_move(playnm);
					}
				} 
				else 
  				makemove(trek, human);
  			markoer = MARKOER_OFF;
  			UpdateDisplay(BRIKS);
    		curcl = other(curcl);
	    } 
	    if (!gameover) {
		    calculating = TRUE;
		    game_logic();
		    if (makelist(&list, human, &mainboard))
		    	markoer = list.move[0];
		    else
		    	markoer = MARKOER_OFF;
		    UpdateDisplay(BRIKS);
		    clon = FALSE;
		  }
	    if (!calculating) {
 				curcl = other(curcl);
 				if (gameover) {
				 	declarewinner(); 
				}
				else
					clon = TRUE;
 			}
			play = FALSE;
		}

	}
	while (!done);
}

/**********************************************/
/*              Initering                     */
/**********************************************/

void init_window(void)
{
	if (calclib)
		return;

	initscr();
	cbreak();
	noecho();
	nonl();
	intrflush(stdscr, FALSE);
	noqiflush();
	refresh(); 
}

void opset(int newval)
{
	if (newval) {
		if (revdef.timedef == tid_per_trek) {
		/*	menu_icheck(M_tree,TIDPTK,TRUE); */
			tid_kontrol = tid_per_trek;
			lookahead = revdef.leveltid;
			lastdyb = revdef.leveldyb;
			lasttid = revdef.leveltid;
		} else {
  /*		menu_icheck(M_tree,SOGE,TRUE); */
  		tid_kontrol = sogedybde;
			lookahead = revdef.leveldyb;
			lastdyb = revdef.leveldyb;
			lasttid = revdef.leveltid;
  	}
  	border_on = revdef.borderdef;
/*  	if ((hvmul = revdef.hvmudef) == TRUE)
  		menu_icheck(M_tree,MULTRK,TRUE);
  	if ((orient = revdef.notadef) == TRUE)
  		menu_icheck(M_tree,VENDB,TRUE);
  	if ((gamopen = revdef.gamdef) == TRUE)
  		menu_icheck(M_tree,GAMWIN,TRUE);
  	if ((anaopen = revdef.anadef) == TRUE)
  		menu_icheck(M_tree,ANAWIN,TRUE); */
  	hvmul = revdef.hvmudef;
  	orient = revdef.notadef;
  	gamopen = revdef.gamdef;
  	anaopen = revdef.anadef;
  } else {
 /* 	menu_icheck(M_tree,TIDPTK,TRUE); */
		tid_kontrol = tid_per_trek;
		lookahead = 3;
		lastdyb = 3;
		lasttid = 3;
		lastspil = 3;
		backthink = FALSE;
		border_on = FALSE;
		hvmul = FALSE;
		orient = FALSE;
		anaopen = FALSE;
		gamopen = FALSE;
	}
}


short int init(void)
{	
	FILE * fd;
	
		clhvid = 0;
		clsort = 0;
		clon = TRUE;
		nytid = TRUE;
		curcl = DARK;
		play = FALSE;
		pass = FALSE;
		calc = FALSE;
		bcalc = FALSE;
		
		tilbage = FALSE;
		frem = FALSE;
		treknu = FALSE;
		
		if ((fd = fopen("rev.cfg","r")) != NULL) {
	    if (fread(&revdef, sizeof(revdef),1,fd) == 1) 
		    opset(TRUE);
		  else
		  	opset(FALSE);
		  fclose(fd);
	  } 
	  else
			opset(FALSE);

		init_nodes(ANTAL_KNUDER);
		
		if (Get_book()) {
      libok = TRUE;
      libon = TRUE;
			tryagain = 0;
    }
    else {
      libok = FALSE;
      libon = FALSE;
			tryagain = 5;
    } 
		
/*		if (Get_book()) {
      libok = FALSE;
      libon = FALSE;
    }
    else {
      libok = FALSE;
      libon = FALSE;
    } */


		signal (SIGINT, SIG_IGN);
		make_node(&root); 

		init_window();
		init_rev();
		init_game(); 
		markoer = list.move[0];
		f_hash_init ();
		UpdateDisplay(ALL);

		
		
#ifdef DEBUG
		debugf = fopen("moves", "w");
#endif

		return TRUE;
}

void clean_up(void)
{
	free(fnode); 
	#ifdef DEBUG
	fclose(debugf);
	#endif

	gotoXY (1, 24);
	clear ();
  refresh();
  nocbreak();
  endwin ();
}


int main (int argc, const char *argv[])
{
	short int ok, argpoi, minmaxlib, sortlib, extlib, playlib,x;
	
	calclib = FALSE;
	minmaxlib = FALSE;
	sortlib = FALSE;
	extlib = FALSE;
	playlib = FALSE;
	
	if (argc > 2) {
    printf("For mange parametre\n");
    return 0;
  }
  else if (argc == 2) {
    argpoi = 0;
    while (argv[1][argpoi] != 0) {
      switch (argv[1][argpoi])
      {
        case 'l' : calclib = TRUE; break;
				case 'm' : minmaxlib = TRUE; break;
				case 's' : sortlib = TRUE; break;
				case 'e' : extlib = TRUE; break;
				case 'p' : playlib = TRUE; break;
        default : printf("Forkert parameter\n"); return 0;
      }
      argpoi++;
    }
  }

		if (minmaxlib || sortlib || extlib || playlib) 
			calclib = TRUE;
		ok = init();
		if (minmaxlib || sortlib || extlib || playlib) 
			calclib = FALSE;
		if (ok) {
			if (calclib) {
				calc_lib();
				Put_book();
			}
			else if (minmaxlib) {
			  calclib = TRUE;
				minmax_lib();
				Put_book();
			}
			else if (sortlib) {
			  calclib = TRUE;
				sort_lib(&book_root);
				Put_book();
			}
			else if (extlib) {
			  calclib = TRUE;
				calc_lib();
				calc_lib();
				extend_lib();
				Put_book();
			}
			else if (playlib) {
			  calclib = TRUE;
				calc_lib();
				calc_lib();
				selfplay();
				Put_book();
			}



			else {
				for (x = 0; x < 10; x++)
					minmax_lib();
				sort_lib(&book_root);
				Spil(); 
				Put_book();
				clean_up();
			}		
		}
	return 0;
}
