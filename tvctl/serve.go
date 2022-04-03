package main

import (
	"flag"
	"log"
	"net/http"
	"os"
	"os/exec"
)

var (
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
	http.HandleFunc("/reward", rewardHandler)
	log.Fatal(http.ListenAndServe(":21359", nil))
}
