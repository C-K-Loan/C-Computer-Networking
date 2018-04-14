#pragma once
#include "cJSON.h"

/* number of attributes of a film */
#define N_FIELDS 6
#define MAX_FILMS 1000
#define MAX_ID_LEN 10

/* indices for film types */
enum {
    film_title,
    film_original_title,
    film_year,
    film_length,
    film_director,
    film_actors
} film_index;

/* available views for json serialization of film */
enum {
    COMPACT,
    FULL
} view_type;

/* model a film as an array, this makes it easy to iterate over its fields,
 * fields can be accessed by film_index enum */
typedef struct {
    char *fields[N_FIELDS];
} film;

/* array of keys used in json serialization of film */
extern char *field_names[N_FIELDS];

void destroy_film(film *f);
int parse_film(char *s, film **f);
cJSON *film_json_view(film *f, char *id, int view);
