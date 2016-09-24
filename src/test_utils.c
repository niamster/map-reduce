#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include <stdlib.h>
#include <time.h>

#include "utils.h"

void test_issep(void) {
    CU_ASSERT_TRUE(is_sep(' '));
    CU_ASSERT_TRUE(is_sep(':'));
    CU_ASSERT_FALSE(is_sep('\0'));
    CU_ASSERT_FALSE(is_sep('/'));
}

typedef struct {
    size_t count;
    char *mem;
    off_t offset;
    char *words;
    size_t woff;
} _test_fchunk_read_d_t;

static void _test_fchunk_read_word_cb(fchunk_t *chunk, void *user) {
    _test_fchunk_read_d_t *d = user;
    char *words;
    ++d->count;
    words = realloc(d->words, d->woff+chunk->count+1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(words);
    d->words = words;
    memcpy(d->words+d->woff, chunk->mem+chunk->offset, chunk->count);
    d->woff += chunk->count;
    d->words[d->woff] = 0;
}

static void _test_fchunk_read_cb(fchunk_t *chunk, void *user) {
    _test_fchunk_read_d_t *d = user;
    memcpy(d->mem+d->offset, chunk->mem+chunk->offset, chunk->count);
    fchunk_read_word(chunk->mem+chunk->offset, chunk->count, _test_fchunk_read_word_cb, d);
    d->offset += chunk->count;
}

void test_fchunk_read(void) {
    const char tbuf[] = "a b c d";
    char *mem = malloc(sizeof(tbuf));
    _test_fchunk_read_d_t d = {
        .count = 0,
        .mem = malloc(sizeof(tbuf)),
        .offset = 0,
        .words = NULL,
        .woff = 0,
    };
    CU_ASSERT_PTR_NOT_NULL_FATAL(mem);
    CU_ASSERT_PTR_NOT_NULL_FATAL(d.mem);
    int jdx; for (jdx=0; jdx<2; ++jdx) {
        int idx; for (idx=1; idx<10; ++idx) {
            d.offset = d.count = 0;
            free(d.words), d.words = NULL;
            d.woff = 0;
            memset(d.mem, 0, sizeof(tbuf));
            CU_ASSERT_EQUAL_FATAL(fchunk_read(tbuf, sizeof(tbuf)-3+jdx, idx, _test_fchunk_read_cb, &d), 0);
            memset(d.mem, 0, sizeof(tbuf));
            memcpy(d.mem, tbuf, sizeof(tbuf)-3+jdx);
            CU_ASSERT_EQUAL_FATAL(d.count, 3);
            CU_ASSERT_EQUAL_FATAL(memcmp(tbuf, d.mem, sizeof(tbuf)-3+jdx), 0);
            CU_ASSERT_PTR_NOT_NULL_FATAL(d.words);
            CU_ASSERT_EQUAL_FATAL(d.woff, 3);
            CU_ASSERT_EQUAL_FATAL(strlen(d.words), 3);
            CU_ASSERT_EQUAL_FATAL(memcmp(d.words, "abc", 3), 0);
        }
    }
    free(d.mem);
    free(mem);
}

int init_suite(void) {
    srand((unsigned)time(NULL));

    return 0;
}

int clean_suite(void) {
    return 0;
}

int main(int argc, char **argv) {
   CU_pSuite suite = NULL;
   (void)argc, (void)argv;

   if (CUE_SUCCESS != CU_initialize_registry())
      return CU_get_error();

   if (!(suite = CU_add_suite("utils", init_suite, clean_suite)))
       goto out;

   if (!(CU_add_test(suite, "issep", test_issep)))
       goto out;
   if (!(CU_add_test(suite, "fchunk_read", test_fchunk_read)))
       goto out;

   CU_basic_run_tests();

  out:
   CU_cleanup_registry();
   return CU_get_error();
}
