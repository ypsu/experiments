package ppladder

import (
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"flag"
	"fmt"
	"hash"
	"html/template"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"syscall"
	"time"
)

var projectnameFlag = flag.String("ppl.projectname", "ppladder", "the name of the project.")
var addressFlag = flag.String("ppl.address", "in our street", "the text to identify the ping pong table.")
var pinFlag = flag.String("ppl.pin", "1234", "pin code for the registration.")
var lockfileFlag = flag.String("ppl.lockfile", "", "lockfile to lock when writing to the datafile.")
var datafileFlag = flag.String("ppl.datafile", "ppladder/data", "datafile to append to.")
var adminAccountFlag = flag.Int("ppl.adminaccount", 10, "the account to consider as admin.")
var faviconFlag = flag.String("ppl.favicon", "ppladder/favicon.ico", "the icon to use as favicon.")

const lossReportCooldown unixmilli = 60 * 60 * 1000

const accountEditLimit = 10
const accountEditReset unixmilli = 60 * 60 * 1000

var ServeMux http.ServeMux

var favicon []byte

func Init() {
	var err error
	favicon, err = os.ReadFile(*faviconFlag)
	if err != nil {
		log.Fatalf("couldn't load favicon: %v", err)
	}

	ServeMux.HandleFunc("/", gstate.serve)
	gstate.init()

	appender := lockedFileAppender{
		file:     *datafileFlag,
		lockfile: *lockfileFlag,
		ps:       make(chan []byte, 8),
	}
	go appender.WriteLoop()
	gstate.dataw = appender

	go gstate.processor()
}

type state struct {
	// time is the timestamp of state's last update.
	time unixmilli

	users    []userdata
	matches  []matchdata
	ranklist []int

	user2id map[string]int

	// writer for the new data entries.
	dataw io.Writer

	// request queue for the processor.
	reqq chan procreq

	// html templates.
	templates *template.Template

	// random number generator for the password seed and cookies.
	randrd io.Reader

	// ratelimit counters.
	accountEdits int
	lastEdit     unixmilli
}

var gstate state

var kTimeFormat = "20060102.150405.000"

type unixmilli int64

type userdata struct {
	// user id numbers start from 10.
	ID                          int
	Name                        string
	pwhash                      string
	Availability, Contact, Note string

	// active browser cookies that allow a login.
	Cookies []cookiedata

	// win/loss stats.
	Wins, Losses      int
	Winlist, Losslist string
	Winset, Lossset   map[int]unixmilli
	MatchIDs          []int
}

type matchdata struct {
	Time              unixmilli
	Winnerid, Loserid int
	Losernote         string
}

type cookiedata struct {
	// cookie data, a string of random chars.
	Data string

	// login time.
	Time unixmilli

	// the browser agent used.
	Agent string
}

type rankdata struct {
	Rank         int
	ID           int
	Name         string
	Wins, Losses int

	// Class is the CSS class for the row.
	Class string
}

type procreq struct {
	w    http.ResponseWriter
	r    *http.Request
	done chan bool
}

// hasquota returns true if a request is safe to proceed.
// if not, it responds on the request and returns false.
func (s *state) hasquota(use bool, pr *procreq) bool {
	lastperiod := s.lastEdit / accountEditReset
	curperiod := s.time / accountEditReset
	if lastperiod != curperiod {
		s.accountEdits = 0
	}
	if s.accountEdits >= accountEditLimit {
		tdata := struct{ Message string }{
			Message: "Server is experiencing a large edit load. Try again an hour later.",
		}
		if err := s.templates.ExecuteTemplate(pr.w, "message.thtml", tdata); err != nil {
			pr.w.WriteHeader(500)
			pr.w.Write([]byte("500 internal error"))
			log.Printf("couldn't render message.thtml: %v", err)
		}
		return false
	}
	if use {
		s.accountEdits++
		s.lastEdit = s.time
	}
	return true
}

var userRE = regexp.MustCompile("^[a-z]{3,12}$")

// line must be without the timestamp, that's added automatically.
func (s *state) addData(line string) {
	if !strings.HasSuffix(line, "\n") {
		line = line + "\n"
	}
	t := time.UnixMilli(int64(s.time)).UTC().Format(kTimeFormat)
	data := t + " " + line
	s.processdata(data)
	if s.dataw != nil {
		if _, err := s.dataw.Write([]byte(data)); err != nil {
			log.Fatalf("couldn't write data %q: %v", data, err)
		}
	}
}

func (s *state) process(pr *procreq) {
	// look up the user from the cookie if available.
	var user *userdata
	sessionCookie, err := pr.r.Cookie("sessionid")
	var cookie string
	if err == nil && sessionCookie != nil {
		var uid int
		if _, err := fmt.Sscanf(sessionCookie.Value, "%d.%s", &uid, &cookie); err == nil {
			if 10 <= uid && uid < len(s.users) {
				for _, cd := range s.users[uid].Cookies {
					if cd.Data == cookie {
						user = &s.users[uid]
						break
					}
				}
			}
		}
	}

	relpath := strings.TrimPrefix(pr.r.URL.Path, "/")
	if relpath == "" {
		tdata := struct {
			Projectname string
			Address     string
			User        string
			Ranklist    []rankdata
			Inactive    []string
		}{
			Projectname: *projectnameFlag,
			Address:     *addressFlag,
			Ranklist:    make([]rankdata, 0, len(s.ranklist)),
		}
		if user != nil {
			tdata.User = user.Name
		}
		for i, ruid := range s.ranklist {
			ru := &s.users[ruid]
			if ru.Wins == 0 && ru.Losses == 0 {
				if ru.Name != "-" {
					tdata.Inactive = append(tdata.Inactive, ru.Name)
				}
				continue
			}
			rd := rankdata{
				Rank:   i + 1,
				ID:     ru.ID,
				Name:   ru.Name,
				Wins:   ru.Wins,
				Losses: ru.Losses,
				Class:  "cnormalrow",
			}
			if user != nil {
				if ru.ID == user.ID {
					rd.Class = "cmyrow"
				} else if user.Winset[ru.ID] > 0 {
					rd.Class = "cwinrow"
				} else if user.Lossset[ru.ID] > 0 {
					rd.Class = "clossrow"
				}
			}
			tdata.Ranklist = append(tdata.Ranklist, rd)
		}
		if err := s.templates.ExecuteTemplate(pr.w, "index.thtml", tdata); err != nil {
			pr.w.WriteHeader(500)
			pr.w.Write([]byte("500 internal error"))
			log.Printf("couldn't render index.thtml: %v", err)
			return
		}
	} else if relpath == "register" {
		if pr.r.Method == "POST" {
			if err := pr.r.ParseForm(); err != nil {
				pr.w.WriteHeader(400)
				fmt.Fprintf(pr.w, "400 couldn't parse form: %v\n", err)
				return
			}
			form := pr.r.Form
			if !userRE.MatchString(form.Get("user")) {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 invalid username\n"))
				return
			}
			if s.user2id[form.Get("user")] != 0 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 username already taken\n"))
				return
			}
			if len(form.Get("password")) == 0 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 missing password\n"))
				return
			}
			if len(form.Get("pin")) == 0 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 missing pin\n"))
				return
			}
			if len(form.Get("avail")) == 0 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 missing availability\n"))
				return
			}
			if len(form.Get("avail")) > 1600 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 availability too long\n"))
				return
			}
			if len(form.Get("contact")) == 0 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 missing contact\n"))
				return
			}
			if len(form.Get("contact")) > 1600 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 contact too long\n"))
				return
			}
			if len(form.Get("note")) > 1600 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 note too long\n"))
				return
			}
			if !s.hasquota(true, pr) {
				return
			}
			if form.Get("pin") != *pinFlag {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 invalid pin\n"))
				return
			}
			data := &strings.Builder{}
			uid := len(s.users)
			fmt.Fprintf(data, "%d", uid)
			fmt.Fprintf(data, " name %s", form.Get("user"))
			fmt.Fprintf(data, " avail %q", form.Get("avail"))
			fmt.Fprintf(data, " contact %q", form.Get("contact"))
			if len(form.Get("note")) > 0 {
				fmt.Fprintf(data, " note %q", form.Get("note"))
			}
			fmt.Fprintf(data, " pwhash %s", s.genPWHash(form.Get("password"), ""))
			cookie := s.randomString(9)
			fmt.Fprintf(data, " addcookie %s %q", cookie, pr.r.Header.Get("User-Agent"))
			s.addData(data.String())
			tdata := struct{ Message string }{
				Message: "Registration successful.",
			}
			secure := ""
			if pr.r.TLS != nil {
				secure = ";Secure"
			}
			pr.w.Header().Set("Set-Cookie", fmt.Sprintf("sessionid=%d.%s;Max-Age=2147483647;SameSite=Strict%s", uid, cookie, secure))
			if err := s.templates.ExecuteTemplate(pr.w, "message.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render message.thtml: %v", err)
				return
			}
		} else {
			if !s.hasquota(false, pr) {
				return
			}
			usernames := []string{}
			for i := 10; i < len(s.users); i++ {
				usernames = append(usernames, s.users[i].Name)
			}
			tdata := struct{ Projectname, Usernames string }{
				Projectname: *projectnameFlag,
				Usernames:   strings.Join(usernames, ","),
			}
			if err := s.templates.ExecuteTemplate(pr.w, "register.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render register.thtml: %v", err)
				return
			}
		}
	} else if relpath == "edit" {
		if user == nil {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("400 not logged in\n"))
			return
		}
		if pr.r.Method == "POST" {
			if err := pr.r.ParseForm(); err != nil {
				pr.w.WriteHeader(400)
				fmt.Fprintf(pr.w, "400 couldn't parse form: %v\n", err)
				return
			}
			form := pr.r.Form
			if !userRE.MatchString(form.Get("user")) {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 invalid username\n"))
				return
			}
			if user.Name != form.Get("user") && s.user2id[form.Get("user")] != 0 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 username already taken\n"))
				return
			}
			if len(form.Get("avail")) == 0 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 missing availability\n"))
				return
			}
			if len(form.Get("avail")) > 1600 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 availability too long\n"))
				return
			}
			if len(form.Get("contact")) == 0 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 missing contact\n"))
				return
			}
			if len(form.Get("contact")) > 1600 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 contact too long\n"))
				return
			}
			if len(form.Get("note")) > 1600 {
				pr.w.WriteHeader(400)
				pr.w.Write([]byte("400 note too long\n"))
				return
			}
			if !s.hasquota(true, pr) {
				return
			}
			data := &strings.Builder{}
			fmt.Fprintf(data, "%d", user.ID)
			changed := false
			if len(form.Get("password")) > 0 {
				changed = true
				fmt.Fprintf(data, " pwhash %s", s.genPWHash(form.Get("password"), ""))
				// invalidate other sessions.
				for _, cd := range user.Cookies {
					if cd.Data != cookie {
						fmt.Fprintf(data, " rmcookie %s", cd.Data)
					}
				}
			}
			if form.Get("user") != user.Name {
				changed = true
				fmt.Fprintf(data, " name %s", form.Get("user"))
			}
			if form.Get("avail") != user.Availability {
				changed = true
				fmt.Fprintf(data, " avail %q", form.Get("avail"))
			}
			if form.Get("contact") != user.Contact {
				changed = true
				fmt.Fprintf(data, " contact %q", form.Get("contact"))
			}
			if form.Get("note") != user.Note {
				changed = true
				fmt.Fprintf(data, " note %q", form.Get("note"))
			}
			message := "Nothing changed."
			if changed {
				s.addData(data.String())
				message = "Changes saved."
			}
			tdata := struct{ Message, Usernames string }{
				Message: message,
			}
			if err := s.templates.ExecuteTemplate(pr.w, "message.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render message.thtml: %v", err)
				return
			}
		} else {
			if !s.hasquota(false, pr) {
				return
			}
			usernames := []string{}
			for i := 10; i < len(s.users); i++ {
				if i != user.ID {
					usernames = append(usernames, s.users[i].Name)
				}
			}
			tdata := struct {
				Projectname, Name, Avail, Contact, Note, Usernames string
			}{*projectnameFlag, user.Name, user.Availability, user.Contact, user.Note, strings.Join(usernames, ",")}
			if err := s.templates.ExecuteTemplate(pr.w, "edit.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render edit.thtml: %v", err)
				return
			}
		}
	} else if relpath == "login" {
		if pr.r.Method == "POST" {
			if err := pr.r.ParseForm(); err != nil {
				pr.w.WriteHeader(400)
				fmt.Fprintf(pr.w, "400 couldn't parse form: %v\n", err)
				return
			}
			form := pr.r.Form
			errorString := ""
			if len(form.Get("user")) == 0 {
				errorString = "Missing username. Try again!"
			}
			if len(errorString) == 0 && len(form.Get("password")) == 0 {
				errorString = "Missing password. Try again!"
			}
			if !s.hasquota(true, pr) {
				return
			}
			uid := s.user2id[form.Get("user")]
			if len(errorString) == 0 && uid == 0 {
				errorString = "Invalid username/password. Try again!"
			}
			if len(errorString) == 0 {
				u := &s.users[uid]
				pwhash := s.genPWHash(form.Get("password"), u.pwhash)
				if pwhash != u.pwhash {
					errorString = "Invalid username/password. Try again!"
				}
			}
			if len(errorString) > 0 {
				tdata := struct{ Error string }{
					Error: errorString,
				}
				if err := s.templates.ExecuteTemplate(pr.w, "login.thtml", tdata); err != nil {
					pr.w.WriteHeader(500)
					pr.w.Write([]byte("500 internal error"))
					log.Printf("couldn't render register.thtml: %v", err)
				}
				return
			}

			// delete old cookies.
			user = &s.users[uid]
			if len(user.Cookies) >= 5 {
				data := &strings.Builder{}
				fmt.Fprintf(data, "%d", uid)
				for len(user.Cookies) >= 5 {
					fmt.Fprintf(data, " rmcookie %s", user.Cookies[0].Data)
					user.Cookies = user.Cookies[1:]
				}
				s.addData(data.String())
			}

			// save the cookie.
			cookie := s.randomString(9)
			s.addData(fmt.Sprintf("%d addcookie %s %q", uid, cookie, pr.r.Header.Get("User-Agent")))
			secure := ""
			if pr.r.TLS != nil {
				secure = ";Secure"
			}
			pr.w.Header().Set("Set-Cookie", fmt.Sprintf("sessionid=%d.%s;Max-Age=2147483647;SameSite=Strict%s", uid, cookie, secure))
			tdata := struct{ Message string }{
				Message: "Login successful.",
			}
			if err := s.templates.ExecuteTemplate(pr.w, "message.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render message.thtml: %v", err)
				return
			}
		} else {
			if !s.hasquota(false, pr) {
				return
			}
			tdata := struct{ Projectname, Error string }{
				Projectname: *projectnameFlag,
				Error:       "",
			}
			if err := s.templates.ExecuteTemplate(pr.w, "login.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render register.thtml: %v", err)
				return
			}
		}
	} else if strings.HasPrefix(relpath, "u/") {
		profilename := relpath[2:]
		var profile *userdata
		if id, err := strconv.Atoi(profilename); err == nil && id >= 10 && id < len(s.users) {
			profile = &s.users[id]
		} else {
			profile = &s.users[s.user2id[profilename]]
		}
		if profile.ID == 0 {
			pr.w.WriteHeader(404)
			pr.w.Write([]byte("404 not found\n"))
			return
		}
		if profile.Name == "-" {
			pr.w.WriteHeader(404)
			pr.w.Write([]byte("404 profile deactivated\n"))
			return
		}
		type historyEntry struct {
			Time     unixmilli
			Type     string
			Opponent string
			Note     string
			Loser    string
		}
		tdata := struct {
			CurrentUser string
			Profile     *userdata
			History     []historyEntry
			CanReport   bool
			IsAdmin     bool
		}{
			Profile: profile,
			History: make([]historyEntry, 0, len(profile.MatchIDs)),
		}
		for i := range profile.MatchIDs {
			m := &s.matches[profile.MatchIDs[len(profile.MatchIDs)-i-1]]
			if s.users[m.Winnerid].Name == "-" || s.users[m.Loserid].Name == "-" {
				continue
			}
			he := historyEntry{
				Time: m.Time,
				Note: m.Losernote,
			}
			he.Loser = s.users[m.Loserid].Name
			if m.Winnerid == profile.ID {
				he.Type = "won"
				he.Opponent = s.users[m.Loserid].Name
			} else {
				he.Type = "lost"
				he.Opponent = s.users[m.Winnerid].Name
			}
			tdata.History = append(tdata.History, he)
		}
		if user != nil {
			tdata.CurrentUser = user.Name
			tdata.CanReport = s.time-user.Lossset[profile.ID] > lossReportCooldown
			tdata.IsAdmin = user.ID == *adminAccountFlag
		}
		if err := s.templates.ExecuteTemplate(pr.w, "user.thtml", tdata); err != nil {
			pr.w.WriteHeader(500)
			pr.w.Write([]byte("500 internal error"))
			log.Printf("couldn't render user.thtml: %v", err)
			return
		}
	} else if relpath == "logout" {
		if user == nil {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("400 not logged in\n"))
			return
		}
		if err := pr.r.ParseForm(); err != nil {
			pr.w.WriteHeader(400)
			fmt.Fprintf(pr.w, "400 couldn't parse form: %v\n", err)
			return
		}
		form := pr.r.Form
		data := &strings.Builder{}
		fmt.Fprintf(data, "%d", user.ID)
		if form.Has("all") {
			for _, cd := range user.Cookies {
				fmt.Fprintf(data, " rmcookie %s", cd.Data)
			}
		} else {
			if len(cookie) == 0 {
				log.Fatal("invalid cookie")
			}
			fmt.Fprintf(data, " rmcookie %s", cookie)
		}
		s.addData(data.String())
		secure := ""
		if pr.r.TLS != nil {
			secure = ";Secure"
		}
		pr.w.Header().Set("Set-Cookie", fmt.Sprintf("sessionid=x;Max-Age=-1;SameSite=Strict%s", secure))
		tdata := struct{ Message string }{
			Message: "Logout successful.",
		}
		if err := s.templates.ExecuteTemplate(pr.w, "message.thtml", tdata); err != nil {
			pr.w.WriteHeader(500)
			pr.w.Write([]byte("500 internal error"))
			log.Printf("couldn't render message.thtml: %v", err)
			return
		}
	} else if relpath == "lost" {
		if user == nil {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("400 not logged in\n"))
			return
		}
		if err := pr.r.ParseForm(); err != nil {
			pr.w.WriteHeader(400)
			fmt.Fprintf(pr.w, "400 couldn't parse form: %v\n", err)
			return
		}
		form := pr.r.Form
		winnerid, err := strconv.Atoi(form.Get("winnerid"))
		if err != nil {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("invalid winnerid\n"))
			return
		}
		if len(form.Get("comment")) > 500 {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("comment too long\n"))
			return
		}
		if winnerid < 10 || winnerid >= len(s.users) || winnerid == user.ID {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("invalid winnerid number\n"))
			return
		}
		if s.time-user.Lossset[winnerid] < lossReportCooldown {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("too frequent reporting\n"))
			return
		}
		s.addData(fmt.Sprintf("%d lostto %d %q", user.ID, winnerid, form.Get("comment")))
		http.Redirect(pr.w, pr.r, "/u/"+s.users[winnerid].Name, 303)
	} else if relpath == "counter" {
		if err := s.templates.ExecuteTemplate(pr.w, "counter.thtml", nil); err != nil {
			pr.w.WriteHeader(500)
			pr.w.Write([]byte("500 internal error"))
			log.Printf("couldn't render counter.thtml: %v", err)
			return
		}
	} else if relpath == "admin" {
		if pr.r.Method != "POST" {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("400 must be post\n"))
			return
		}
		if user == nil || user.ID != *adminAccountFlag {
			pr.w.WriteHeader(400)
			pr.w.Write([]byte("400 not admin\n"))
			return
		}
		if err := pr.r.ParseForm(); err != nil {
			pr.w.WriteHeader(400)
			fmt.Fprintf(pr.w, "400 couldn't parse form: %v\n", err)
			return
		}
		form := pr.r.Form
		profileid, err := strconv.Atoi(form.Get("userid"))
		if err != nil {
			pr.w.WriteHeader(400)
			fmt.Fprintf(pr.w, "400 couldn't userid: %v\n", err)
			return
		}
		if profileid < 10 || profileid >= len(s.users) {
			pr.w.WriteHeader(400)
			fmt.Fprintf(pr.w, "400 userid %d out of limit of %d", profileid, len(s.users))
			return
		}
		profile := &s.users[profileid]
		if form.Get("op") == "pwreset" {
			// log out all existing sessions.
			data := &strings.Builder{}
			fmt.Fprintf(data, "%d", profile.ID)
			for _, cd := range profile.Cookies {
				fmt.Fprintf(data, " rmcookie %s", cd.Data)
			}

			// set up the new password.
			pw := s.randomString(9)
			fmt.Fprintf(data, " pwhash %s", s.genPWHash(pw, ""))
			s.addData(data.String())

			// report the new password.
			tdata := struct{ Message string }{
				Message: "New password: " + pw,
			}
			if err := s.templates.ExecuteTemplate(pr.w, "message.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render message.thtml: %v", err)
				return
			}
		} else if form.Get("op") == "deactivate" {
			s.addData(fmt.Sprintf("%d name -", profile.ID))
			tdata := struct{ Message string }{
				Message: "Account deactivated.",
			}
			if err := s.templates.ExecuteTemplate(pr.w, "message.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render message.thtml: %v", err)
				return
			}
		} else if form.Get("op") == "ratelimitreset" {
			s.lastEdit = 0
			tdata := struct{ Message string }{
				Message: "Ratelimit counters reset.",
			}
			if err := s.templates.ExecuteTemplate(pr.w, "message.thtml", tdata); err != nil {
				pr.w.WriteHeader(500)
				pr.w.Write([]byte("500 internal error"))
				log.Printf("couldn't render message.thtml: %v", err)
				return
			}
		}
	} else if relpath == "favicon.ico" {
		pr.w.Header().Set("Cache-Control", "max-age: 604800")
		pr.w.Write(favicon)
	} else {
		pr.w.WriteHeader(404)
		pr.w.Write([]byte("404 not found\n"))
	}
}

func (s *state) processor() {
	datafile, err := os.ReadFile(*datafileFlag)
	if err != nil {
		log.Fatalf("couldn't open data file: %v", err)
	}
	if err := s.processdata(string(datafile)); err != nil {
		log.Fatalf("couldn't initialize state from data: %v", err)
	}
	for req := range s.reqq {
		// ensure that the timestamps are unique in the write operations.
		now := unixmilli(time.Now().UnixMilli())
		if now <= s.time {
			now = s.time + 1
		}
		s.time = now
		s.process(&req)
		req.done <- true
	}
}

func (s *state) init() {
	s.users = make([]userdata, 10)
	s.reqq = make(chan procreq, 4)
	s.randrd = rand.Reader
	s.user2id = map[string]int{}

	// load the html templates.
	matches, err := filepath.Glob("*.thtml")
	if err != nil {
		log.Fatalf("globbing error: %v", err)
	}
	moreMatches, err := filepath.Glob("ppladder/*.thtml")
	if err != nil {
		log.Fatalf("globbing error: %v", err)
	}
	matches = append(matches, moreMatches...)
	s.templates, err = template.ParseFiles(matches...)
	if err != nil {
		log.Fatalf("couldn't load html templates: %v", err)
	}
}

func (s *state) processdata(data string) error {
	needsrank := false
	for lineid, line := range strings.Split(data, "\n") {
		lineid++
		line = strings.TrimSpace(line)
		if len(line) == 0 || line[0] == '#' {
			continue
		}

		// find the user for this line.
		rd := strings.NewReader(line)
		var timestampstr string
		var userid int
		if _, err := fmt.Fscan(rd, &timestampstr, &userid); err != nil {
			return fmt.Errorf("error parsing time/user on line %d: %v", lineid, err)
		}
		tm, err := time.Parse(kTimeFormat, timestampstr)
		if err != nil {
			return fmt.Errorf("error parsing time on line %d: %v", lineid, err)
		}
		millis := unixmilli(tm.UnixMilli())
		if userid == len(s.users) {
			s.users = append(s.users, userdata{ID: userid})
		}
		if userid < 10 || userid > len(s.users) {
			return fmt.Errorf("userid %d out of limit of %d", userid, len(s.users))
		}
		u := &s.users[userid]

		// parse the actions.
		var action string
		for {
			if _, err := fmt.Fscan(rd, &action); err != nil {
				break
			}
			switch action {
			case "name":
				oldname := u.Name
				if _, err := fmt.Fscan(rd, &u.Name); err != nil {
					return err
				}
				delete(s.user2id, oldname)
				if u.Name != "-" {
					s.user2id[u.Name] = userid
				}
				needsrank = true
			case "pwhash":
				if _, err := fmt.Fscan(rd, &u.pwhash); err != nil {
					return err
				}
			case "avail":
				if _, err := fmt.Fscanf(rd, "%q", &u.Availability); err != nil {
					return err
				}
			case "contact":
				if _, err := fmt.Fscanf(rd, "%q", &u.Contact); err != nil {
					return err
				}
			case "note":
				if _, err := fmt.Fscanf(rd, "%q", &u.Note); err != nil {
					return err
				}
			case "addcookie":
				var cookie, agent string
				if _, err := fmt.Fscanf(rd, "%s %q", &cookie, &agent); err != nil {
					return err
				}
				cd := cookiedata{
					Data:  cookie,
					Time:  millis,
					Agent: agent,
				}
				u.Cookies = append(u.Cookies, cd)
			case "rmcookie":
				var cookie string
				if _, err := fmt.Fscan(rd, &cookie); err != nil {
					return err
				}
				for i, cd := range u.Cookies {
					if cd.Data != cookie {
						continue
					}
					u.Cookies = append(u.Cookies[:i], u.Cookies[i+1:]...)
					break
				}
			case "lostto":
				var winnerid int
				var losernote string
				if _, err := fmt.Fscanf(rd, "%d %q", &winnerid, &losernote); err != nil {
					return err
				}
				if winnerid < 10 || winnerid >= len(s.users) || winnerid == u.ID {
					return fmt.Errorf("bad winnerid %d on line %d", winnerid, lineid)
				}
				md := matchdata{
					Time:      millis,
					Winnerid:  winnerid,
					Loserid:   u.ID,
					Losernote: losernote,
				}
				s.users[winnerid].MatchIDs = append(s.users[winnerid].MatchIDs, len(s.matches))
				s.users[u.ID].MatchIDs = append(s.users[u.ID].MatchIDs, len(s.matches))
				s.matches = append(s.matches, md)
				needsrank = true
			default:
				return fmt.Errorf("unrecognized action %q on line %d", action, lineid)
			}
		}
	}
	if needsrank {
		s.rank()
	}
	return nil
}

func (s *state) serve(w http.ResponseWriter, r *http.Request) {
	req := procreq{w, r, make(chan bool)}
	s.reqq <- req
	<-req.done
}

func (s *state) rank() {
	wins := map[int]map[int]unixmilli{}
	losses := map[int]map[int]unixmilli{}
	for i := 10; i < len(s.users); i++ {
		wins[i] = map[int]unixmilli{}
		losses[i] = map[int]unixmilli{}
	}

	for _, md := range s.matches {
		if s.time-md.Time > 365*24*60*60*1000 {
			continue
		}
		if s.users[md.Winnerid].Name == "-" || s.users[md.Loserid].Name == "-" {
			continue
		}
		delete(losses[md.Winnerid], md.Loserid)
		delete(wins[md.Loserid], md.Winnerid)
		wins[md.Winnerid][md.Loserid] = md.Time
		losses[md.Loserid][md.Winnerid] = md.Time
	}

	ranklist := []*userdata{}
	for i := 10; i < len(s.users); i++ {
		u := &s.users[i]
		ranklist = append(ranklist, u)
		u.Wins = len(wins[i])
		u.Losses = len(losses[i])
		u.Winset = map[int]unixmilli{}
		u.Lossset = map[int]unixmilli{}
		list := []string{}
		for uid, time := range wins[i] {
			u.Winset[uid] = time
			list = append(list, s.users[uid].Name)
		}
		sort.Strings(list)
		u.Winlist = strings.Join(list, ", ")
		list = []string{}
		for uid, time := range losses[i] {
			u.Lossset[uid] = time
			list = append(list, s.users[uid].Name)
		}
		sort.Strings(list)
		u.Losslist = strings.Join(list, ", ")
	}

	sort.Slice(ranklist, func(i, j int) bool {
		if ranklist[i].Wins != ranklist[j].Wins {
			return ranklist[i].Wins > ranklist[j].Wins
		}
		if ranklist[i].Losses != ranklist[j].Losses {
			return ranklist[i].Losses > ranklist[j].Losses
		}
		return ranklist[i].Name < ranklist[j].Name
	})

	if len(s.ranklist) != len(ranklist) {
		s.ranklist = make([]int, len(ranklist))
	}
	for i, r := range ranklist {
		s.ranklist[i] = r.ID
	}
}

var hasher hash.Hash

// genPWHash generates a hash from a password.
// If oldhash is not empty, it should be a previously returned hash.
// genPWHash will extract the salt from it to generate the same hash.
func (s *state) genPWHash(pw, oldhash string) string {
	var salt []byte
	if len(oldhash) > 0 {
		var err error
		salt, err = hex.DecodeString(oldhash[0:32])
		if err != nil || len(salt) != 16 {
			log.Fatalf("couldn't decode salt from %q: %v", oldhash, err)
		}
	} else {
		salt = make([]byte, 16)
		if _, err := s.randrd.Read(salt); err != nil {
			log.Fatalf("couldn't generate salt: %v", err)
		}
	}
	if hasher == nil {
		hasher = sha256.New()
	}
	hasher.Reset()
	hasher.Write(salt)
	hasher.Write([]byte("."))
	hasher.Write([]byte(pw))
	h := hasher.Sum(nil)
	return hex.EncodeToString(salt) + "." + hex.EncodeToString(h)
}

func (s *state) randomString(length int) string {
	const chars = "acbdefghijklmnopqrstuvwxyzACBDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
	str := make([]byte, length)
	if _, err := s.randrd.Read(str); err != nil {
		log.Fatalf("couldn't generate random string: %v", err)
	}
	for i := 0; i < length; i++ {
		str[i] = chars[int(str[i])%len(chars)]
	}
	return string(str)
}

// lockedFileAppender is an io.Writer
// that appends to a file in the background.
// Thanks to the background writing it is non-blocking.
// When there's nothing to write, it keeps the file closed.
// It only writes to the file under a lock if lockfile is nonempty.
type lockedFileAppender struct {
	file     string
	lockfile string
	ps       chan []byte
}

func (w lockedFileAppender) Write(p []byte) (int, error) {
	tp := make([]byte, len(p))
	copy(tp, p)
	w.ps <- tp
	return len(p), nil
}

func (w lockedFileAppender) WriteLoop() {
	for p := range w.ps {
		var lockfile *os.File
		if len(w.lockfile) > 0 {
			lockfile, _ = os.Open(w.lockfile)
			if lockfile != nil {
				if err := syscall.Flock(int(lockfile.Fd()), syscall.LOCK_EX); err != nil {
					log.Printf("couldn't lock %q: %v", w.lockfile, err)
				}
			}
		}

		f, err := os.OpenFile(w.file, os.O_WRONLY|os.O_APPEND, 0o600)
		if err != nil {
			log.Fatalf("couldn't open %q: %v", w.file, err)
		}
		if n, err := f.Write(p); err != nil || n != len(p) {
			log.Fatalf("only wrote %d out of %d to %q: %v", n, len(p), w.file, err)
		}

		f.Close()
		if lockfile != nil {
			lockfile.Close()
		}
	}
}
