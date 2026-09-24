/*************************************************************************/
/*                                                                       */
/*  reversi.h, prototyper og datastrukturer til brug i de forskellige    */
/*  moduler                                                              */
/*                                                                       */
/*************************************************************************/

/* #include <sys/time.h> */

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif 

#define MAXMOVES 57
#define NORMAL_SEARCH 0
#define EXTENTED_SEARCH 1
#define TRESH 3

#define other( x )  (1 - x)

short int getcomputer(void);

enum contents {LIGHT, DARK, EMPTY, BORDER, MARKER};

enum tid_type {sogedybde, tid_per_trek, spil_tid};

/****************************************************************/
/* movelist strukturen bruges til at opbevare en liste af treak */
/****************************************************************/
typedef struct {
            char nmoves;
            char move[MAXMOVES];
            } movelist;

/*******************************************************************/
/*  breadt representation "board", sq bruges som et 10x10 array    */
/* af felter. Vi har her vores breadt (8x8) omringet af greanse-   */
/* felter. Dette goer det lettere at skrive vores treak algoritmer */
/* ndiscs er antallet af brikker for henholdsvis sort og hvid.     */
/* possible er en liste af treak, hvor det er muligt at enten hvid */
/* eller sort kan treakke                                          */
/*******************************************************************/
typedef struct {
            char sq[100];
            char ndiscs[2];
            movelist possible;
            } board;

typedef struct {
            short int sidste;
            char moves[80];
            board boards[80];
            } gamerec;

typedef struct {
            short int sidste;
            short int first_move;
            char moves[80];
            board first_pos;
            } savegame;

typedef struct {
            short int sidste;
            char moves[80];
            } save1game;

typedef struct tnode {
            struct tnode *barn;
            struct tnode *sosk;
            short int move;
            } tree;

enum direction {NORTH, NORTHEAST, EAST, SOUTHEAST,
                 SOUTH, SOUTHWEST, WEST, NORTHWEST};

extern short int delta[8],sofar,bestmove,varlook,b1calc,allway,value,bvalue;
extern short int search_type,currentbest,thinking,posible;
extern board  mainboard;
extern gamerec game;
extern savegame sgame;
extern short int playnm,lookahead,computer,human,gameover,hvmul,calc,bcalc;
extern movelist list;
extern long aeval,aknud,gam_tid;
extern long GameTid;
extern struct timeval tv;


/******************************************************************/
/*  procedurer og variable til opdatering af breadt og trekliste  */
/*                       modul treak.c                            */
/******************************************************************/
extern short int dirs [89][16],dirs1[89][16];
int inlist(short int b, movelist *l);
void makemove(short int k, short int pl);
char makelist(movelist *legal, short int pl, board *bd);
short int countmov(short int pl, board *bd,short int potential[]);
void trymove(short int trysq, short int pl, board *bd);
void trylist(movelist *legal,short int trysq, short int pl, board *bd);
void ftrylist(movelist *legal,short int trysq, short int pl, board *bd);
void ftrymove(short int trysq, short int pl, board *bd);
char fasteval(short int pl, board *bd);
short int legalmove(short int k, board *bd, short int pl);
char legalmoves(short int pl, board *bd);
char legaleval(short int pl, board *bd);
void init_rev(void);
void init_game(void);

/***************************************/
/*  minimax procedurer og variable     */
/*         modul minmax.c              */
/***************************************/

short int findmax2(short int player,
            short int look,
            short int depth,
            short int killer,
            movelist *list,
            board *bd,
            short int alfa,
            short int beta,
            short int *bestmove);
short int findmax1(short int player,
            short int look,
            short int depth,
            short int killer,
            movelist *list,
            board *bd,
            short int alfa,
            short int beta,
            short int *bestmove,
            tree* *spiltree);
short int findmax(short int player,
            short int look,
            short int depth,
            board *bd,
            short int alfa,
            short int beta,
            short int *bestmove,
            tree* *spiltree);

short int zero_findmax(short int player,
						short int look,
            short int depth,
            board *bd,
            short int alfa,
            short int beta,
            short int *bestmove,
            tree* *spiltree);

char slutmax(short int player,
             short int look,
             short int depth,
             board *bd,
             char alfa,
             char beta,
             short int *bestmove,
             tree* *spiltree);
#ifdef FASTSLUT
char slutmax3(short int player,
						 board *bd,
             char alfa,
             char beta);

char slutmax2(board *bd,
             char alfa,
             char beta,
             short int *bestmove);

char slutmax1(short int player,
             short int look,
             short int depth,
             board *bd,
             char alfa,
             char beta,
             short int *bestmove,
             short int killer);
#else
char slutmin1(short int look,
              movelist *list,
             board *bd,
             char alfa,
             char beta,
             short int *bestmove,
             short int killer);
char slutmax1(short int look,
              movelist *list,
             board *bd,
             char alfa,
             char beta,
             short int *bestmove,
             short int killer);
#endif
char zero_slutmax(short int player,
             short int look,
             short int depth,
             board *bd,
             char alfa,
             char beta,
             short int *bestmove,
             tree* *spiltree);

/******************************************/
/*  evaluerings procedurer og variable    */
/*         modul minmax.c                 */
/******************************************/
short int eval(short int alfa,
         short int beta,
         short int oppmov,
         short int player,
         board *bd);

/*******************************/
/* adminstration af spil trae  */
/*        modul trae.c         */
/*******************************/
            
#define ANTAL_KNUDER 300000l    /* saa mange knuder gemmes i vores trae */  
     
extern tree *root,*nfree,*fnode,*memslut,*blackroot,*whiteroot;
extern short int treeon,hurtig,animer,libon,libok,moremoves;
extern long aalloc,maxalloc;

void init_nodes(long antal);
void make_node(tree* *spiltree); 
void list_in_tree(movelist *list,tree* *spiltree);
void put_in_front(tree* *start, tree* *cur);
void free_nodes(tree *rt);
short int bcopytree(tree* *rt);
short int copytree(tree* *rt);
/* int Get_lib(char * whitelib, char * blacklib); */

/****************************************/
/* procedurer og variable til sortering */
/*            modul sort.c              */
/****************************************/

extern int humres[78][78],comres[78][78]; 
void delres(int *res);
void simsort(short int kill,short int player,board *bd, movelist *list);
void sortlist(short int k, short int kill, short int pl, movelist *list, board *bd);

/*****************************************************/
/*  procedurer og variable til brug for tidskontrol  */
/*                 modul kontrol.c                   */
/*****************************************************/

extern long tider[15],rtider[15],spil_tider[15],timeleft[2],
       timemove,lowtimemove,timesleft[100];
extern enum tid_type tid_kontrol;
extern long starttid,sluttid,search_time, timemove;
short int stop_nu(short int varlook);
/* int gettimeofday(struct timeval *tp, struct timezone *tzp); */
void make_res(short int value, long tid);
void make_try(short int move,short int look);

/*****************************************************/
/*  procedurer og variable til brug for hashing      */
/*                 modul hash.c                      */
/*****************************************************/
typedef struct {
					long a0,a1;
	} hash_num;

void f_hash_init (void);
#ifdef __GNUC__
inline
#endif
void 
f_hash_put (char d, char f, short int v, short int yx, hash_num hash_numbers);
#ifdef __GNUC__
inline
#endif
short int 
f_hash_get (char d, char * f, short int * v, hash_num hash_numbers);
void f_hash_board (int player, hash_num * hash_numbers, board * b);



/**********************************/
/*    tabeller for sidevaerdier   */
/*          modul kanter.c        */
/**********************************/

extern short int sikker[6561], white_v[6561], white_h[6561], black_v[6561], black_h[6561],hjo_trek[6561];
