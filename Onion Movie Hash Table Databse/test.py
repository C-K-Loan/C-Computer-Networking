#/bin/python3

import requests
from random import randint
from json import dumps

FILM_KEYS = [
    "title",
    "originalTitle",
    "productionYear",
    "length",
    "director",
    "actors"
]
FILMS_CSV = "filme.csv"
FILM_COLLECTION_URL = 'http://localhost:4711/films'
N_ITEMS = 5

post_data = dict((FILM_KEYS[i], str(i)) for i in range(len(FILM_KEYS)))
put_data = dict((FILM_KEYS[i], str(i+10)) for i in range(len(FILM_KEYS)))

def film_to_dict(film_values):
    assert(len(film_values) == len(FILM_KEYS))
    return dict(zip(FILM_KEYS, film_values))

def random_films():
    films = []

    for i in range(N_ITEMS):
        film = dict((key, str(randint(0, 100))) for key in FILM_KEYS)
        film["title"] = str(i)
        films.append(film)

    return films

def assert_film_equals(film_json, film):
    for key in FILM_KEYS:
        assert film_json[key] == film[key], key

def test_real_post():
    clean_database()
    films = []
    with open(FILMS_CSV, 'r') as f:
        for line in f:
            data = film_to_dict(line.strip().split(';'))
            films.append(data)

    for film in films:
        res = requests.post(FILM_COLLECTION_URL, json=film)
        assert res.status_code == 201, "could not post film to collection"
        location = res.headers.get('location')
        film_json = requests.get(location).json()
        assert_film_equals(film_json, film)

    res = requests.get(FILM_COLLECTION_URL).json()
    assert len(res['items']) == len(films)

def clean_database():
    requests.delete(FILM_COLLECTION_URL)
    res = requests.get(FILM_COLLECTION_URL).json()
    assert len(res['items']) == 0, "couldn't delete collection"

def test_delete_collection():
    clean_database()

    post_films = random_films()
    for post_data in post_films:
        requests.post(FILM_COLLECTION_URL, json=post_data)

    coll = requests.get(FILM_COLLECTION_URL).json()
    assert len(coll['items']) == N_ITEMS, len(coll['items'])
    requests.delete(FILM_COLLECTION_URL)
    coll = requests.get(FILM_COLLECTION_URL).json()
    assert len(coll['items']) == 0

def test_post():
    clean_database()
    post_films = random_films()

    for post_data in post_films:
        res = requests.post(FILM_COLLECTION_URL, json=post_data)
        assert res.status_code == 201
        location = res.headers.get('location')
        film_json = requests.get(location).json()
        assert_film_equals(film_json, post_data)

    coll = requests.get(FILM_COLLECTION_URL).json()
    assert len(coll['items']) == N_ITEMS, len(coll['items'])

    for post_data in post_films:
        res = requests.post(FILM_COLLECTION_URL, json=post_data)
        assert res.status_code == 404

    coll = requests.get(FILM_COLLECTION_URL).json()
    assert len(coll['items']) == N_ITEMS

def test_put_resource():
    clean_database()
    post_films = random_films()
    put_films = random_films()

    for i in range(N_ITEMS):
        post_data = post_films[i]
        location = requests.post(FILM_COLLECTION_URL, json=post_data).headers['location']
        put_data = put_films[i]
        res = requests.put(location, data="data={}".format(dumps(put_data)))
        assert res.status_code == 200
        film_json = requests.get(location).json()
        assert_film_equals(film_json, put_data)

    coll = requests.get(FILM_COLLECTION_URL).json()
    assert len(coll['items']) == N_ITEMS

def test_delete_resource():
    clean_database()
    post_films = random_films()

    for post_data in post_films:
        requests.post(FILM_COLLECTION_URL, json=post_data)

    coll = requests.get(FILM_COLLECTION_URL).json()
    items = coll['items']
    assert len(items) == N_ITEMS

    for i in range(len(items)):
        item = items[i]
        href = item['href']
        requests.delete(href)
        assert requests.get(href).status_code == 404
        new_coll = requests.get(FILM_COLLECTION_URL).json()
        assert len(new_coll['items']) == N_ITEMS - (i + 1)

def test_post_wrong():
    res = requests.post(FILM_COLLECTION_URL, data="post_data")
    assert res.status_code == 404
    post_data = {'Länge': 3}
    res = requests.post(FILM_COLLECTION_URL, json=post_data)
    assert res.status_code == 404

test_post()
test_put_resource()
test_delete_resource()
test_real_post()
test_delete_collection()
test_post_wrong()
