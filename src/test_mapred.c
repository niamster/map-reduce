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

static void _basic_map(mr_t *mr, ukey_t *key, void *user) {
    int res;
    (void)user;

    res = mr_emit_intermediate(mr, key, NULL);
    if (res != 0)
        die("Failed to emit a key: %d\n", res);
}

static void _basic_reduce(mr_t *mr, ukey_t *key, olentry_t *entries, olentry_t *values, void *user) {
    unsigned long count = 0;
    olentry_t *el = entries;
    (void)user;

    while (true) {
        ++count;
        if (el->next == -1)
            break;
        el = &values[el->next];
    }

    mr_emit(mr, key, (void *)count);

}

static void _basic_output(ukey_t *key, olentry_t *entries, olentry_t *values, void *user) {
    olentry_t *el = entries;
    _basic_data_t *bdata = user;
    char tmp[32];
    int res;
    (void)values;

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
    const char text[] = "foo bar qux\nbar bar baz";
    const char expect[] = "bar=3\nbaz=1\nfoo=1\nqux=1\n";
    mr_t mr;

    unsigned t; for (t=1; t<10; ++t) {
        _basic_data_t bdata = {
            .out = NULL,
            .len = 0,
        };
        CU_ASSERT_EQUAL_FATAL(mr_init(&mr, t), 0);
        CU_ASSERT_EQUAL_FATAL(mr_process(&mr, text, sizeof(text), _basic_map, _basic_reduce, &bdata), 0);
        CU_ASSERT_EQUAL_FATAL(wtable_iterate(&mr.output, _basic_output, &bdata), 0);
        mr_destroy(&mr);

        CU_ASSERT_PTR_NOT_NULL_FATAL(bdata.out);
        assert(bdata.out);
        CU_ASSERT_STRING_EQUAL_FATAL(bdata.out, expect);
        free(bdata.out);
    }
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