#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include <stdlib.h>
#include <sys/param.h>
#include <time.h>
#include <assert.h>

#include "mapred.h"

void test_init(void) {
    mr_t mr;

    CU_ASSERT_EQUAL_FATAL(mr_init(&mr, 1), 0);
    mr_destroy(&mr);
}

typedef struct {
    char *out;
    size_t len;
} _basic_data_t;

static void _basic_map(mr_t *mr, const char *data, size_t size, void *user) {
    (void)user;

    fchunk_read_word(data, size, _mr_chunk_process_word, mr);
}

static void _basic_reduce(mr_t *mr, ukey_t *key, unsigned long entry, void *user) {
    unsigned long count = 0;
    olentry_t el;
    int res;
    (void)user;

    res = mr_get_entry(mr, key, entry, &el);
    if (res != 0)
        die("Failed to get entry: %d\n", res);

    while (true) {
        ++count;
        if (el.next == OLIST_NONE)
            break;

        res = mr_get_entry(mr, key, el.next, &el);
        if (res != 0)
            die("Failed to get entry: %d\n", res);
    }

    mr_emit(mr, key, (void *)count);

}

static void _basic_output(ukey_t *key, olentry_t *el, void *user) {
    _basic_data_t *bdata = user;
    char tmp[32];
    int res;

    res = snprintf(tmp, sizeof(tmp), "=%lu\n", (unsigned long)el->value);

    char *out = realloc(bdata->out, bdata->len + key->len + res + 1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(out);
    assert(out);
    bdata->out = out;

    memcpy(bdata->out+bdata->len, key->key, key->len);
    bdata->len += key->len;

    memcpy(bdata->out+bdata->len, tmp, res);
    bdata->len += res;

    bdata->out[bdata->len] = 0;
}

void test_basic(void) {
    const char template[] = "foo bar qux\nbar bar baz sdf yui ";
    const unsigned tlen = sizeof(template) - 1;
    const unsigned copies = 1000;
    char *text, *expect = NULL;
    mr_t mr;

    text = calloc(1, tlen*copies + 1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(text);
    assert(text);
    unsigned idx; for (idx=0; idx<copies; ++idx) {
        memcpy(text + tlen*idx, template, tlen);
    }
    asprintf(&expect, "bar=%u\nbaz=%u\nfoo=%u\nqux=%u\nsdf=%u\nyui=%u\n",
        3*copies, 1*copies, 1*copies, 1*copies, 1*copies, 1*copies);
    CU_ASSERT_PTR_NOT_NULL_FATAL(expect);
    assert(expect);

    unsigned t; for (t=1; t<10; ++t) {
        _basic_data_t bdata = {
            .out = NULL,
            .len = 0,
        };
        CU_ASSERT_EQUAL_FATAL(mr_init(&mr, t), 0);
        CU_ASSERT_EQUAL_FATAL(mr_process(&mr, text, strlen(text)+1, _basic_map, _basic_reduce, _basic_output, &bdata), 0);
        mr_destroy(&mr);

        CU_ASSERT_PTR_NOT_NULL_FATAL(bdata.out);
        assert(bdata.out);
        CU_ASSERT_STRING_EQUAL_FATAL(bdata.out, expect);
        free(bdata.out);
    }

    free(expect);
    free(text);
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

   if (!(suite = CU_add_suite("mapred", init_suite, clean_suite)))
       goto out;

   if (!(CU_add_test(suite, "init", test_init)))
       goto out;
   if (!(CU_add_test(suite, "basic", test_basic)))
       goto out;

   CU_basic_run_tests();

  out:
   CU_cleanup_registry();
   return CU_get_error();
}
