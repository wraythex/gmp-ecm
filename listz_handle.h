#include <gmp.h>
#include "basicdefs.h"
#include "ecm-impl.h"

/* This type is the basis for file I/O of mpz_t */
typedef unsigned long file_word_t;

typedef struct {
  int storage; /* memory = 0, file = 1 */
  uint64_t len;
  size_t words; /* Number of file_word_t in a residue */
  union {
    listz_t mem;
    FILE *file;
  } data;
  char *filename;
} _listz_handle_t;
typedef _listz_handle_t *listz_handle_t;


/* The only permissible access modes for listz_iterator_*() functions are read-only, 
   write-only, and read-then-write to each residue in sequence. */ 

typedef struct{
  listz_handle_t handle;
  file_word_t *buf;
  size_t bufsize; /* Size of buffer, in units of file_word_t's */
  uint64_t offset; /* First buffered element's offset relative to 
                    start of file, units of file_word_t's */
  size_t valid; /* Number of valid entries in buffer */
  uint64_t readptr, writeptr; /* In unit of file_word_t's, relative to 
                               current buf. If handle is stored in memory,
                               points at the next mpz_t to read or write, 
                               resp. */
  int dirty;
} listz_iterator_t;

listz_handle_t listz_handle_init (const char *, uint64_t, const mpz_t);
void listz_handle_clear (listz_handle_t);
void listz_handle_get (listz_handle_t, mpz_t, file_word_t *, uint64_t);
void listz_handle_get2 (listz_handle_t, mpz_t, uint64_t);
void listz_handle_set (listz_handle_t, const mpz_t, file_word_t *, uint64_t);
void listz_handle_output_poly (const listz_handle_t, uint64_t, int, int, const char *, const char *, int);

listz_iterator_t *listz_iterator_init (listz_handle_t, uint64_t);
listz_iterator_t *listz_iterator_init2 (listz_handle_t, uint64_t, uint64_t);
void  listz_iterator_clear (listz_iterator_t *);
void listz_iterator_read (listz_iterator_t *, mpz_t);
void listz_iterator_write (listz_iterator_t *, const mpz_t);
