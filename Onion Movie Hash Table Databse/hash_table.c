#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "hash_table.h"

#define INITIAL_SIZE 3

/* calculate a hash value of a char array */
size_t string_hash(void *p_in, size_t str_len) {
    char *str = p_in;
    size_t hash = 5381;
    for (size_t i = 0; i < str_len; i++) {
        int c = str[i];
        hash = ((hash << 5) + hash) + (size_t) c;  /* hash * 33 + c */
    }

    return hash;
}

/* allocate and initialize a hash table */
hash_table *ht_create() {
    hash_table *tbl = malloc(sizeof *tbl);
    tbl->size = INITIAL_SIZE;
    tbl->n_elems = 0;
    tbl->elems = calloc(tbl->size, sizeof *(tbl->elems));

    return tbl;
}

/* get the index in the tbl->elems array for a given key */
size_t get_hash(hash_table *tbl, void *key, size_t key_len) {
    return string_hash(key, key_len) % tbl->size;
}

/* get the value corresponding to a key,
 * *res is a pointer to the value, belongs to table, so don't free the pointer
 * length of the result is stored in res_len*/
int ht_get_value(hash_table *tbl, void *key, size_t key_len, void **res, size_t *res_len) {
    hash_table_elem *list = tbl->elems[get_hash(tbl, key, key_len)];

    while (list != NULL) {
        if (list->key_len == key_len && memcmp(list->key, key, key_len) == 0) {
            *res = list->value;
            *res_len = list->value_len;
            return 0;
        }
        list = list->next;
    }

    *res = NULL;
    *res_len = 0;
    return -1;
}

/* double the size of the array and rehash elements */
void resize(hash_table *tbl) {
    size_t prev_size = tbl->size;
    tbl->size *= 2;
    hash_table_elem **new_elems = calloc(tbl->size, sizeof *new_elems);

    for (size_t i = 0; i < prev_size; i++) {
        hash_table_elem *list = tbl->elems[i];

        while (list != NULL) {
            size_t hash = get_hash(tbl, list->key, list->key_len);
            hash_table_elem *next = list->next;
            list->next = new_elems[hash];
            new_elems[hash] = list;
            list = next;
        }
    }

    free(tbl->elems);
    tbl->elems = new_elems;
}

/* set a value of a given key, idempotent */
int ht_set_value(hash_table *tbl, void *key, size_t key_len, void *value, size_t value_len) {
    /* if 3/4 of the elements are full, resize table */
    if (4 * tbl->n_elems > 3 * tbl->size) {
        resize(tbl);
    }

    size_t hash = get_hash(tbl, key, key_len);
    hash_table_elem *list = tbl->elems[hash];

    while (list != NULL) {
        if (list->key_len == key_len && memcmp(list->key, key, key_len) == 0) {
            free(list->value);
            /* allocate buffer big enough to hold str + NUL-byte */
            /* always write NUL-byte */
            list->value = malloc(value_len);
            memcpy(list->value, value, value_len);
            list->value_len = value_len;

            return 0;
        }
        list = list->next;
    }

    hash_table_elem *new_elem = malloc(sizeof *new_elem);

    new_elem->key = malloc(key_len);
    memcpy(new_elem->key, key, key_len);
    new_elem->key_len = key_len;

    new_elem->value = malloc(value_len);
    memcpy(new_elem->value, value, value_len);
    new_elem->value_len = value_len;

    new_elem->next = tbl->elems[hash];
    tbl->elems[hash] = new_elem;
    tbl->n_elems++;

    return 0;
}

/* remove key from hash table */
int ht_delete_key(hash_table *tbl, void *key, size_t key_len) {
    size_t hash = get_hash(tbl, key, key_len);
    hash_table_elem *list = tbl->elems[hash];

    hash_table_elem *prev = NULL;
    while (list != NULL) {
        if (list->key_len == key_len && memcmp(list->key, key, key_len) == 0) {
            if (prev == NULL) {
                assert(tbl->elems[hash] == list);
                tbl->elems[hash] = list->next; /* set head to next element */
            } else {
                prev->next = list->next;
            }

            free(list->key);
            free(list->value);
            tbl->n_elems--;
            free(list);
            return 0;
        }

        prev = list;
        list = list->next;
    }

    return -1;
}

/* destroy and free hash table */
void ht_destroy(hash_table *tbl) {
    for (size_t i = 0; i < tbl->size; i++) {
        hash_table_elem *list = tbl->elems[i];
        while (list != NULL) {
            hash_table_elem *elem = list->next;
            free(list->key);
            free(list->value);
            free(list);
            list = elem;
        }
    }

    free(tbl->elems);
    free(tbl);
}
