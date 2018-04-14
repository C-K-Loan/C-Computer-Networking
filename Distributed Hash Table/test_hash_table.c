#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "hash_table.h"

#define N_TESTS 100000
#define BUFFER_LEN 10

int main() {
    char *res;
    hash_table *tbl = ht_create();
    ht_set_value(tbl, "key1", 4, "value1", 6);
    ht_set_value(tbl, "key2", 4, "value2", 6);
    ht_set_value(tbl, "key3", 4, "value3", 6);

    size_t res_len;
    assert(tbl->n_elems == 3);
    assert(ht_get_value(tbl, "key1", 4, &res, &res_len) == 0);
    assert(memcmp(res, "value1", 6) == 0);
    assert(ht_get_value(tbl, "key2", 4, &res, &res_len) == 0);
    assert(memcmp(res, "value2", 6) == 0);
    assert(ht_get_value(tbl, "key3", 4, &res, &res_len) == 0);
    assert(memcmp(res, "value3", 6) == 0);

    assert(ht_get_value(tbl, "non-existant", 12, &res, &res_len) == -1);

    ht_set_value(tbl, "key1", 4, "new-value", 9);
    assert(tbl->n_elems == 3);
    assert(ht_get_value(tbl, "key1", 4, &res, &res_len) == 0);
    assert(memcmp(res, "new-value", 9) == 0);

    assert(ht_delete_key(tbl, "key2", 4) == 0);
    assert(ht_delete_key(tbl, "key3", 4) == 0);
    assert(ht_delete_key(tbl, "non-existant", 12) == -1);
    assert(tbl->n_elems == 1);
    ht_destroy(tbl);

    tbl = ht_create();
    /* get, insert, get, update */
    size_t expected_elems = 0;
    for (int i = 0; i < N_TESTS; i++) {
        char key[BUFFER_LEN];
        char value[BUFFER_LEN];

        sprintf(key, "%d", i);
        sprintf(value, "a%d", i);
        size_t key_len = strlen(key);
        size_t value_len = strlen(value);

        char *res = NULL;
        size_t res_len = 0;
        assert(ht_get_value(tbl, key, key_len, &res, &res_len) == -1 && res == NULL);

        assert(ht_set_value(tbl, key, key_len, value, value_len) == 0);
        expected_elems++;
        assert(expected_elems == tbl->n_elems);
        assert(ht_get_value(tbl, key, key_len, &res, &res_len) == 0);
        assert(memcmp(value, res, value_len) == 0);

        sprintf(value, "b%d", i);
        value_len = strlen(value);
        assert(ht_set_value(tbl, key, key_len, value, value_len) == 0);
        assert(expected_elems == tbl->n_elems);
        assert(ht_get_value(tbl, key, key_len, &res, &res_len) == 0);
        assert(memcmp(value, res, value_len) == 0);
    }

    /* delete, delete, get */
    for (int i = 0; i < N_TESTS; i++) {
        char key[BUFFER_LEN];
        sprintf(key, "%d", i);
        size_t key_len = strlen(key);

        assert(ht_delete_key(tbl, key, key_len) == 0);
        expected_elems--;
        assert(expected_elems == tbl->n_elems);

        assert(ht_delete_key(tbl, key, key_len) == -1);
        assert(expected_elems == tbl->n_elems);

        char *res = NULL;
        size_t res_len = 0;
        assert(ht_get_value(tbl, key, key_len, &res, &res_len) == -1);
        assert(res == NULL);
    }
    ht_destroy(tbl);

    printf("all tests passed.\n");
}
