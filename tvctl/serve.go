package main

import (
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/exec"
)

var (
	port       = flag.Int("port", 21359, "the port for the server.")
	rootPath   = flag.String("root", "", "if set, serve files from this dir.")
	rewardPath = flag.String("reward", "./reward", "path to the reward script.")
)

func rewardHandler(w http.ResponseWriter, req *http.Request) {
	if req.Method != "POST" {
		return
	}
	w.Header().Set("Access-Control-Allow-Origin", "*")
	go func() {
		cmd := exec.Command(*rewardPath)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		log.Print("got a reward request.")
		log.Print("reward completed: ", cmd.Run())
	}()
}

func main() {
	flag.Parse()
	if len(*rootPath) > 0 {
		fs := http.FileServer(http.Dir(*rootPath))
		http.HandleFunc("/", func(w http.ResponseWriter, req *http.Request) {
			if req.URL.Path == "/" {
				w.WriteHeader(404)
				return
			}
			w.Header().Set("Cache-Control", "no-cache")
			fs.ServeHTTP(w, req)
		})
	}
	http.HandleFunc("/reward", rewardHandler)
	log.Fatal(http.ListenAndServe(fmt.Sprintf(":%d", *port), nil))
}
