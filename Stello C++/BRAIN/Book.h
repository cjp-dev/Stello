
#define CALCULATED 0x1
#define EXACT 0x2
#define INEXACT 0x4

#define WUN_GAME 0
#define DRAW_GAME 1
#define LOST_GAME 2
#define NOT_KNOWN 3
#define NOT_CALCULATED 4
#define C1ALCULATED 5 


typedef struct bnode {
            struct bnode *barn;
            struct bnode *sosk;
            short int move;
						short int value;
						short int flag;
            } booktree;
						
extern booktree *book_root,*nbfree,*fbnode;

int Get_book(void);
int Put_book(void);
void init_book_nodes(long antal);
short int getlib(short int pl, short int * bstmov, short int movenum, movelist *list, short int * score);
short int play_game(board * bd, short int x, gamerec * gam);

void calc_lib(void);
void minmax_lib(void);
void sort_lib(booktree ** root);
void extend_lib(void);
void mmgame(short int depth,
						short int win,
							booktree ** root);
void convert_game(void);
void selfplay(void);
booktree * getlibpos(short int movenum);

