#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "film.h"

/* field names for json representation of film datastructure */
char *field_names[N_FIELDS] = {
    "title",
    "originalTitle",
    "productionYear",
    "length",
    "director",
    "actors"
};

/* deallocate film and it's members */
void destroy_film(film *f) {
    for (int i = 0; i < N_FIELDS; i++) {
        if (f->fields[i] != NULL) {
            free(f->fields[i]);
        }
    }

    free(f);
}

/* create (and allocate) film from json string
 * set *f to the new film
 * return 0 if parsing worked, else 1.
 * on error set *f = NULL */
int parse_film(char *s, film **f) {
    cJSON *root = cJSON_Parse(s);
    *f = calloc(1, sizeof *(*f));

    for (int i = 0; i < N_FIELDS; i++) {
        char *field_name = field_names[i];
        cJSON *field = cJSON_GetObjectItem(root, field_name);

        if (field == NULL || field->valuestring == NULL) {
            destroy_film(*f);
            *f = NULL;
            cJSON_Delete(root);
            return 1;
        }

        int len = strlen(field->valuestring);
        char *field_value = calloc(len + 1, sizeof *field_value);
        strncpy(field_value, field->valuestring, len);
        field_value[len] = '\0';
        (*f)->fields[i] = field_value;
    }

    cJSON_Delete(root);
    return 0;
}

/* return json string for film. includes href attribute, which is set to *id,
 * 2 different "views" are available:
 * COMPACT: only Titel & href,
 * FULL: all field values of the film (see field_names) & href */
cJSON *film_json_view(film *f, char *id, int view) {
    assert(f != NULL);

    cJSON *obj = cJSON_CreateObject();

    if (view == COMPACT) {
        cJSON_AddStringToObject(obj, field_names[film_title], f->fields[film_title]);
    } else if (view == FULL) {
        for (int i = 0; i < N_FIELDS; i++) {
            cJSON_AddStringToObject(obj, field_names[i], f->fields[i]);
        }
    } else {
    }

    cJSON_AddStringToObject(obj, "href", id);
    return obj;
}

