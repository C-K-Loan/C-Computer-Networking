#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* get a film from the database by its _id_ (aka some string that contains an integer)
 * return NULL on error */
film *db_get(database *db, char *key, int key_len) {
    int index;
    sscanf(key, "%d", &index);

    if (index >= db->cur_films || index < 0) {
        return NULL;
    }

    film *f = db->films[index];
    return f;
}

/* check if the database contains a film (by title) */
bool db_contains(database *db, char *film_title, int film_title_len) {
    int *id;
    size_t res_len;
    ht_get_value(db->id_map, film_title, film_title_len, &id, &res_len);

    return id != NULL;
}

/* get the corresponding _database id_ for a film title */
int db_get_id(database *db, char *film_title, int film_title_len) {
    int *id;
    size_t res_len;
    ht_get_value(db->id_map, film_title, film_title_len, &id, &res_len);

    if (id == NULL) { /* FIXME maybe assertion */
        return -1;
    }

    assert(res_len == sizeof(*id)); /* assert its only one integer */
    return *id;
}

/* add a film to the database */
void db_add(database *db, char *title, int title_len, film *value) {
    ht_set_value(db->id_map, title, title_len, &(db->cur_films), sizeof(db->cur_films));
    db->films[db->cur_films] = value;
    db->cur_films++;
}

/* update a film entry */
void db_update(database *db, char *title, int title_len, film *value) {
    int id = db_get_id(db, title, title_len);
    film *prev_film = db->films[id];

    char *prev_title = prev_film->fields[film_title];
    int prev_title_len = strlen(prev_title);

    ht_delete_key(db->id_map, prev_title, prev_title_len);
    ht_set_value(db->id_map, title, title_len, &id, sizeof(id));

    destroy_film(prev_film);

    db->films[id] = value;
}

/* delete a film from the database (by id!)*/
void db_delete(database *db, char *id, int id_len) {
    int index;
    sscanf(id, "%d", &index);

    if (index >= db->cur_films || index < 0) {
        return;
    }

    film *f = db->films[index];

    if (f == NULL) {
        return;
    }

    db->films[index] = NULL;
    char *title = f->fields[film_title];
    int title_len = strlen(title);
    ht_delete_key(db->id_map, title, title_len);
    destroy_film(f);
}

/* free database and the containing objects */
void db_destroy(database *db) {
    for (int i = 0; i < db->cur_films; i++) {
        if (db->films[i] != NULL) {
            destroy_film(db->films[i]);
        }
        db->films[i] = NULL;
    }

    db->cur_films = 0;
    ht_destroy(db->id_map);
    free(db);
}

/* create an empty database object */
database *db_create() {
    database *db = calloc(1, sizeof(*db));
    db->id_map = ht_create();

    return db;
}
