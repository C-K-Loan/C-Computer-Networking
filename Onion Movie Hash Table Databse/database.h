#pragma once

#include <stdbool.h>
#include "film.h"
#include "hash_table.h"

typedef struct database {
    film *films[MAX_FILMS];
    hash_table *id_map;
    int cur_films;
} database;

film *db_get(database *db, char *key, int key_len);
bool db_contains(database *db, char *film_title, int film_title_len);
int db_get_id(database *db, char *film_title, int film_title_len);
void db_add(database *db, char *key, int key_len, film *value);
void db_update(database *db, char *title, int title_len, film *value);
void db_delete(database *db, char *key, int key_len);
void db_destroy(database *db);
database *db_create();
