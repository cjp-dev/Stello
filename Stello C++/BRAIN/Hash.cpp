#include "hash.h"
#include <malloc.h>
#include <stdlib.h>

#include "reversi.h"


int c_hash_no;
int c_hash_no1;
long HashCall, HashHit;


t_hentry *v_hentry = 0;

void f_hash_init (void)
{
  register long i,j,z;

  c_hash_no = (1 << HASHSIZE);
  c_hash_no1 = c_hash_no - 1;

  v_hentry = (t_hentry *)malloc (sizeof (v_hentry[0]) * c_hash_no);

  srand (0x1234567);
  srand (rand ());
  srand (rand ());
  srand (rand ());

	H_B[0].a0 =
		(H_B[0].a0p2 = rand ()) ^
		(H_B[0].a0p1 = rand ());
	H_B[0].a1 =
		(H_B[0].a1p1 = rand ()) ^
		(H_B[0].a1p2 = rand ());

	
	for (i = 10; i <= 80; i += 10)
    for (j = 1; j <= 8; j++) {
			z = i + j;
			H_B[z].a0 =
				(H_B[z].a0p2 = rand ()) ^
				(H_B[z].a0p1 = rand ());
			H_B[z].a1 =
				(H_B[z].a1p1 = rand ()) ^
				(H_B[z].a1p2 = rand ());
  }

  for (i = 0; i < c_hash_no; i++) {
    v_hentry[i].f = XX_HASH;
    v_hentry[i].d = 0;
    v_hentry[i].yx = 0;
    v_hentry[i].v = 0;
    v_hentry[i].a1 = 0;
  }

}

#ifdef __GNUC__
inline 
#endif
short int
f_hash_put (char d, char f, short int v, short int yx, hash_num hash_numbers)
{
  register t_hentry *h = &(v_hentry[m_hash_adr (hash_numbers.a0)]);

  if (h->a1 == hash_numbers.a1 && h->d > d)
    return h->yx;
  h->f = f;
  h->d = d;
  h->v = v;
  h->a1 = hash_numbers.a1;
  return (h->yx = yx);
}

/*
 *
 * get from hash
 *
 */

#ifdef __GNUC__
inline
#endif
short int 
f_hash_get (char d, char * f, short int * v, hash_num hash_numbers)
{
  register t_hentry *h = &(v_hentry[m_hash_adr (hash_numbers.a0)]);

  HashCall++;
  if (h->a1 != hash_numbers.a1)
    return 0;
  if (h->d < d) {
    *f = XX_HASH;
    return h->yx;
  }
  HashHit++;
  *f = h->f;
  *v = h->v;
  return h->yx;
}

void f_hash_board (int player, hash_num * hash_numbers, board * b)
{
  register int i,j,z;
  register long a0,a1;

  if (player == DARK) {
    a0 = H_B[0].a0p1;
    a1 = H_B[0].a1p1;
  } else {
    a0 = H_B[0].a0p2;
    a1 = H_B[0].a1p2;
  }
	for (i = 10; i <= 80; i += 10)
    for (j = 1; j <= 8; j++) {
			z = i + j;
			switch (b->sq[z]) {
			case DARK:
				a0 ^= H_B[z].a0p1;
				a1 ^= H_B[z].a1p1;
				break;
			case LIGHT:
				a0 ^= H_B[z].a0p2;
				a1 ^= H_B[z].a1p2;
				break;
			default:
				break;
    	}
		}

  hash_numbers->a0 = a0;
  hash_numbers->a1 = a1;
}


