DATA='{
    "title":    "Filmtitel",
    "originalTitle":    "Originaltitle",
    "productionYear":   "Herstellungsjahr",
    "length":   "Länge",
    "director": "Regie",
    "actors":   "DarstellerInnen"
}'

curl -v -X GET http://localhost:4711/films
curl -v -H "Content-Type: application/json" -X POST -d "$DATA" http://localhost:4711/films
curl -v -X PUT -d "data=$DATA" http://localhost:4711/films/0
curl -v -X DELETE http://localhost:4711/films
