package main

import (
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"strings"
)

var (
	port      = flag.Int("port", 21359, "the port for the server.")
	rootPath  = flag.String("root", "", "if set, serve files from this dir.")
	showsPath = flag.String("shows", "", "directory where the shows live.")
)

func respond(w http.ResponseWriter, code int, format string, v ...any) {
	log.Printf(format, v...)
	w.WriteHeader(code)
	fmt.Fprintf(w, format, v...)
}

func rewardHandler(w http.ResponseWriter, req *http.Request) {
	if req.Method != "POST" {
		return
	}
	w.Header().Set("Access-Control-Allow-Origin", "*")

	// find the next episode for the show in the body.
	body, err := io.ReadAll(req.Body)
	if err != nil {
		respond(w, http.StatusUnprocessableEntity, "read body: %v", err)
		return
	}
	show := strings.ToLower(string(body))
	if show == "" {
		respond(w, http.StatusBadRequest, "missing request body")
		return
	}
	log.Printf("received request for %q", show)
	showdir := filepath.Join(*showsPath, show)
	glob := filepath.Join(showdir, "*/*")
	episodes, err := filepath.Glob(glob)
	if err != nil {
		respond(w, http.StatusInternalServerError, "glob: %v", err)
		return
	}
	if len(episodes) == 0 {
		respond(w, http.StatusInternalServerError, "glob %s: no episodes found", glob)
		return
	}
	sort.Strings(episodes)
	episode := episodes[0]

	// mark the file as seen.
	dst := filepath.Join(showdir, filepath.Base(episode))
	err = os.Rename(episode, dst)
	if err != nil {
		respond(w, http.StatusInternalServerError, "%v", err)
		return
	}
	respond(w, http.StatusOK, "ok")

	// play the file in the background.
	// todo: read mpv customization.
	go func() {
		cmd := exec.Command("mpv", dst)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		log.Printf("running mpv %q.", dst)
		log.Print("mpv completed: ", cmd.Run())
	}()
}

func run() error {
	logfile, err := os.OpenFile(filepath.Join(os.Getenv("HOME"), ".tvctl.log"), os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0755)
	if err != nil {
		return fmt.Errorf("open logfile: %v", err)
	}
	log.SetOutput(io.MultiWriter(logfile, os.Stderr))

	flag.Parse()

	if *showsPath == "" {
		return errors.New("missing --shows")
	}

	if *rootPath == "" {
		log.Print("warn: missing --root, won't serve fs.")
	}
	if *rootPath != "" {
		fs := http.FileServer(http.Dir(*rootPath))
		http.HandleFunc("/", func(w http.ResponseWriter, req *http.Request) {
			if req.URL.Path == "/" {
				w.WriteHeader(404)
				return
			}
			w.Header().Set("Cache-Control", "no-store")
			fs.ServeHTTP(w, req)
		})
	}
	http.HandleFunc("/reward", rewardHandler)

	log.Printf("serving at :%d", *port)
	return http.ListenAndServe(fmt.Sprintf(":%d", *port), nil)
}

func main() {
	if err := run(); err != nil {
		log.Fatal(err)
	}
}
