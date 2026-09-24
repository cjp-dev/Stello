#define OK_HASH (0)
#define LO_HASH (1)
#define HI_HASH (2)
#define XX_HASH (3)

#define HASHSIZE 19

#define m_hash_adr(a) ( a & ( (1<<HASHSIZE) - 1 ) )


typedef struct {
      char f;
      char d;
      short int yx;
      short int v;
      long a1;
   } t_hentry;
	 
typedef struct {
					long a0,a1,a0p1,a0p2,a1p1,a1p2;
	} hash_square;



	
typedef struct {
            hash_square B[100];
            } hash_board;

extern hash_square H_B[100];

extern int c_hash_no, c_hash_no1;
extern long HashCall, HashHit,TransHit;

extern t_hentry *v_hentry;


