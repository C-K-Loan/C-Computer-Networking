/** Licensed under AGPL 3.0. (C) 2010 David Moreno Montero. http://coralbits.com */
#include <onion/onion.h>
#include <onion/dict.h>
#include <onion/block.h>
#include <onion/log.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <netdb.h>
#include <string.h>

#include "cJSON.h"
#include "hash_table.h"
#include "database.h"
#include "film.h"

/* FIXME better way to do this? */
#define HOSTNAME "0.0.0.0"
#define PORT "4711"
#define BASE_URL ("http://" HOSTNAME ":" PORT)

/* FIXME */
static database *db;

/* write a json object to the response and add Mime-Type & Content-Type header to response */
void response_write_json(onion_response *res, cJSON *obj) {
    assert(obj != NULL);

    char *json_str = cJSON_Print(obj);
    onion_response_write0(res, json_str);

    free(json_str);
    onion_response_set_header(res, "Content-Type", "application/json");
    onion_response_set_header(res, "Mime-Type", "application/json");
}

/* json representation film database, items contains a list of films in compact view */
int list_films(void *p, onion_request *req, onion_response *res) {
    cJSON *root;
    cJSON *items;
    root = cJSON_CreateObject();
    items = cJSON_CreateArray();

    int buffer_size = strlen(BASE_URL) + strlen(onion_request_get_fullpath(req)) + 1;
    char *collection_href = calloc(buffer_size, sizeof *collection_href);
    snprintf(collection_href, buffer_size, "%s%s", BASE_URL, onion_request_get_fullpath(req));
    cJSON_AddItemToObject(root, "items", items);

    /* FIXME iteration! */
    for (int i = 0; i < db->cur_films; i++) {
        film *f = db->films[i];

        if (f != NULL) {
            int buffer_size = strlen(collection_href) + MAX_ID_LEN + 1;
            char *film_href = calloc(buffer_size, sizeof *film_href);
            snprintf(film_href, buffer_size, "%s/%d", collection_href, i);

            cJSON *f_json = film_json_view(f, film_href, COMPACT);
            cJSON_AddItemToArray(items, f_json);
            free(film_href);
        }
    }

    cJSON_AddStringToObject(root, "href", collection_href);

    response_write_json(res, root);

    free(collection_href);
    cJSON_Delete(root);

    onion_response_set_code(res, HTTP_OK); /* HTTP 200 */
    return 0;
}

/* 404 error code */
int invalid_request(void *p, onion_request *req, onion_response *res) {
    const char *s = "request failed\n";
    onion_response_write(res, s, strlen(s));
    onion_response_set_code(res, HTTP_NOT_FOUND);

    return OCS_PROCESSED;
}

/* add new film to database, input has to be json */
int add_film(void *p, onion_request *req, onion_response *res) {
    film *f;
    char *data = onion_block_data(onion_request_get_data(req));

    ONION_INFO(data);

    int status = parse_film(data, &f);

    if (status < 0 || f == NULL) {
        return invalid_request(p, req, res);
    }

    char *name = f->fields[film_title];
    int name_len = strlen(name);

    if (db_contains(db, name, name_len)) {
        return invalid_request(p, req, res);
    }

    db_add(db, name, name_len, f);
    int id = db_get_id(db, name, name_len);

    /* create location of new film */
    int buffer_size = strlen(BASE_URL) + strlen(onion_request_get_fullpath(req)) + 1;
    char *collection_href = calloc(buffer_size, sizeof *collection_href);
    snprintf(collection_href, buffer_size, "%s%s", BASE_URL, onion_request_get_fullpath(req));
    buffer_size = strlen(collection_href) + MAX_ID_LEN + 1;
    char *location = calloc(buffer_size, sizeof *location);
    snprintf(location, buffer_size, "%s/%d", collection_href, id);
    location[buffer_size - 1] = '\0';

    onion_response_set_code(res, HTTP_CREATED); /* HTTP 200 */
    onion_response_set_header(res, "Location", location);
    return OCS_PROCESSED;
}

/* delete all films from database */
int reset_db(void *p, onion_request *req, onion_response *res) {
    db_destroy(db);
    db = db_create();

    return OCS_PROCESSED;
}

/* handle http requests to film database
 * GET return json representation of film database, each film is in COMPACT view
 * POST add new film to collection
 * DELETE delete all films from database
 * else return 404 */
int film_collection(void *p, onion_request *req, onion_response *res) {
    onion_request_flags method_flags = onion_request_get_flags(req) & OR_METHODS;

    if (method_flags == OR_GET) {
        list_films(p, req, res);
    } else if (method_flags == OR_POST) {
        /* if film already in table -> exists */
        /* add film to hash table */
        add_film(p, req, res);
    } else if (method_flags == OR_DELETE) {
        /* delete all films in collection */
        return reset_db(p, req, res);
    } else {
        /* method not supported */
        return invalid_request(p, req, res);
    }

    return OCS_PROCESSED;
}

/* get json representation of a film */
int film_resource_get(void *p, onion_request *req, onion_response *res, film *f) {
    assert(f != NULL);
    char *full_path = onion_request_get_fullpath(req);
    int buffer_size = strlen(BASE_URL) + strlen(full_path) + 1;
    char *film_href = calloc(buffer_size, sizeof *film_href);
    snprintf(film_href, buffer_size, "%s%s", BASE_URL, full_path);
    ONION_INFO(film_href);

    cJSON *f_json = film_json_view(f, film_href, FULL);

    response_write_json(res, f_json);

    cJSON_Delete(f_json);
    free(film_href);

    return OCS_PROCESSED;
}

/* update a film in the database. id and data are taken from req */
int film_resource_update(void *p, onion_request *req, onion_response *res, film *f) {
    assert(onion_request_get_flags(req) & OR_PUT);
    assert(f != NULL);
    film *updated_film;

    /* FIXME usually you should not need to send data=... in the request */
    char *data=onion_request_get_put(req,"data");
    if (data == NULL) {
        return 0;
    }

    ONION_INFO(data);

    int status = parse_film(data, &updated_film);

    if (status < 0 || f == NULL) {
        return invalid_request(p, req, res);
    }

    char *title = f->fields[film_title];
    int title_len = strlen(title);
    db_update(db, title, title_len, updated_film);

    return OCS_PROCESSED;
}

/* delete a film from the database by the id given in the request */
int film_resource_delete(void *p, onion_request *req, onion_response *res, film *f) {
    assert(f != NULL);
    char *id = onion_request_get_query(req, "1");
    db_delete(db, id, strlen(id));

    return OCS_PROCESSED;
}

/* handle requests on a film resource
 * GET returns a json representation
 * PUT updates the film
 * DELETE removes the film from the database
 * anything else is ignored and 404 is returned */
int film_resource(void *p, onion_request *req, onion_response *res) {
    char *id = onion_request_get_query(req, "1");
    film *f = db_get(db, id, strlen(id));

    if (f == NULL) {
        return invalid_request(p, req, res);
    }

    onion_request_flags method_flags = onion_request_get_flags(req) & OR_METHODS;

    if (method_flags == OR_GET) {
        /* get film from database */
        return film_resource_get(p, req, res, f);
    } else if (method_flags == OR_PUT) {
        /* if film already in table -> exists */
        /* add film to hash table */
        return film_resource_update(p, req, res, f);
    } else if (method_flags == OR_DELETE) {
        /* delete film */
        return film_resource_delete(p, req, res, f);
    } else {
        /* method not supported */
        return invalid_request(p, req, res);
    }

    return OCS_PROCESSED;
}

onion *o=NULL;

static void shutdown_server(int _){
	if (o)
		onion_listen_stop(o);
}

/* start a film database server listening on PORT
 * the film collection can be found at HOSTNAME:PORT/films
 * a film resource can be found at HOSTNAME:PORT/films/<id> */
int main(int argc, char **argv){
	signal(SIGINT,shutdown_server);
	signal(SIGTERM,shutdown_server);

	o=onion_new(O_POOL);
	onion_set_timeout(o, 5000);
	onion_set_hostname(o, HOSTNAME);
	onion_set_port(o, PORT);
	onion_url *urls=onion_root_url(o);

    onion_url_add(urls, "^films$", film_collection);
    onion_url_add(urls, "^films/(..*)$", film_resource);

    db = db_create();
	onion_listen(o);
	onion_free(o);
    db_destroy(db);
	return 0;
}
