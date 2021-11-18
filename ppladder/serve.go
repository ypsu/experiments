package main

import (
	"flag"
	"ppladder/ppladder"
	"log"
	"net/http"
)

func main() {
	flag.Parse()
	ppladder.Init()
	log.Print("starting server on :8080")
	log.Fatal(http.ListenAndServe(":8080", &ppladder.ServeMux))
}
