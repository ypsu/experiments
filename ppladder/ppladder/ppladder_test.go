package ppladder

import (
	"bytes"
	"flag"
	"fmt"
	"io"
	"math/rand"
	"net/http"
	"net/url"
	"os"
	"regexp"
	"strings"
	"testing"
	"time"
)

var (
	rewriteFlag = flag.Bool("rewrite", false, "rewrite ppladder_test.data if changed.")
)

type record struct {
	key, value string
}

func parseRecords(data string) []record {
	rs := []record{}
	r := record{}
	for _, line := range strings.Split(data, "\n") {
		if len(line) == 0 {
			r.value += "\n"
			continue
		}
		if strings.HasPrefix(line, "  ") {
			r.value += line[2:] + "\n"
			continue
		}
		if len(r.key) != 0 {
			rs = append(rs, r)
		}
		r = record{key: line}
	}
	if len(r.key) != 0 {
		rs = append(rs, r)
	}
	// trim excessive newlines at the end of the values.
	for i := range rs {
		for strings.HasSuffix(rs[i].value, "\n\n") {
			rs[i].value = rs[i].value[0 : len(rs[i].value)-1]
		}
	}
	return rs
}

var lineStartRE = regexp.MustCompile("(?m)^")

func formatRecords(rs []record) string {
	str := strings.Builder{}
	for _, r := range rs {
		str.WriteString(r.key)
		str.WriteByte('\n')
		if len(r.value) > 0 {
			indented := lineStartRE.ReplaceAllString(r.value, "  ")
			str.WriteString(strings.TrimRight(indented, "\n "))
			str.WriteByte('\n')
		}
	}
	return str.String()
}

func formatState(s *state) string {
	r := &strings.Builder{}
	for _, u := range s.users {
		if len(u.Name) == 0 {
			continue
		}
		fmt.Fprintf(r, "user %d %s", u.ID, u.Name)
		if len(u.pwhash) > 0 {
			fmt.Fprintf(r, " pwhash %s", u.pwhash)
		}
		for _, field := range []struct{ k, v string }{
			{"avail", u.Availability},
			{"contact", u.Contact},
			{"note", u.Note},
		} {
			if len(field.v) > 0 {
				fmt.Fprintf(r, " %s %q", field.k, field.v)
			}
		}
		r.WriteByte('\n')
		for _, cd := range u.Cookies {
			t := time.UnixMilli(int64(cd.Time)).UTC().Format(kTimeFormat)
			fmt.Fprintf(r, "user %d cookie %s %s %q\n", u.ID, cd.Data, t, cd.Agent)
		}
	}
	for _, m := range s.matches {
		t := time.UnixMilli(int64(s.time)).UTC().Format(kTimeFormat)
		winner := s.users[m.Winnerid].Name
		loser := s.users[m.Loserid].Name
		fmt.Fprintf(r, "match time %s winner %s loser %s losernote %q\n", t, winner, loser, m.Losernote)
	}
	return r.String()
}

func TestFromData(t *testing.T) {
	dataBytes, err := os.ReadFile("ppladder_test.data")
	if err != nil {
		t.Fatal(err)
	}
	data := string(dataBytes)
	testRecords := parseRecords(data)
	for i := range testRecords {
		t.Run(testRecords[i].key, func(t *testing.T) {
			s := &state{}
			s.init()
			s.randrd = rand.New(rand.NewSource(0))
			dataw := &strings.Builder{}
			s.dataw = dataw
			ntr := []record{}
			processErr := ""
			for _, r := range parseRecords(testRecords[i].value) {
				keyFields := strings.Fields(r.key)
				directive := keyFields[0]
				if directive == "inputdata" {
					ntr = append(ntr, r)
					if err := s.processdata(r.value); err != nil {
						processErr = err.Error()
					}
				} else if directive == "inputquery" {
					if len(keyFields) < 2 {
						t.Fatalf("bad inputquery %q", r.key)
					}
					ntr = append(ntr, r)
					u, err := url.Parse(keyFields[1])
					if err != nil {
						t.Fatalf("invalid query string in %q: %v", r.key, err)
					}
					request := &http.Request{URL: u}
					if len(keyFields) == 3 {
						request.Method = "POST"
						request.Header = http.Header{}
						request.Header.Set("Content-Type", "application/x-www-form-urlencoded")
						request.Body = io.NopCloser(strings.NewReader(keyFields[2]))
					}
					response := &testResponse{hdr: http.Header{}}
					dataw.Reset()
					s.process(&procreq{w: response, r: request})
					if len(dataw.String()) != 0 {
						ntr = append(ntr, record{"outputdata", dataw.String()})
					}
					ntr = append(ntr, record{"outputhtml", response.buf.String()})
				}
			}
			if len(processErr) != 0 {
				ntr = append(ntr, record{"outputdataerr", processErr})
			} else {
				ntr = append(ntr, record{"outputstate", formatState(s)})
			}
			testRecords[i].value = formatRecords(ntr)
		})
	}
	newData := formatRecords(testRecords)
	if data == newData {
		return
	}
	if !*rewriteFlag {
		t.Fatal("found diffs in test output, use --rewrite.")
	}
	os.WriteFile("ppladder_test.data", []byte(newData), 0600)
}

type testResponse struct {
	hdr http.Header
	buf bytes.Buffer
}

func (r *testResponse) Header() http.Header           { return r.hdr }
func (*testResponse) WriteHeader(int)                 {}
func (r *testResponse) Write(buf []byte) (int, error) { return r.buf.Write(buf) }
