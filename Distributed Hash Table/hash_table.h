#pragma once
typedef struct hash_table_elem {
    void *key;
    size_t key_len;
    void *value;
    size_t value_len;
    struct hash_table_elem *next;
} hash_table_elem;

typedef struct hash_table {
    size_t size;
    size_t n_elems;
    hash_table_elem **elems;
} hash_table;

hash_table *ht_create();
int ht_get_value(hash_table *tbl, void *key, size_t key_len, void **res, size_t *res_len);
int ht_set_value(hash_table *tbl, void *key, size_t key_len, void *value, size_t value_len);
int ht_delete_key(hash_table *tbl, void *key, size_t key_len);
void ht_destroy(hash_table *tbl);
