// tvlock is a simple puzzle/challenge; it only exits when the challenge was correctly solved.
package main

import (
	"fmt"
	"math/rand/v2"
	"os"
	"time"
)

func main() {
	if err := run(); err != nil {
		fmt.Printf("Error: %v\n", err)
		os.Exit(1)
	}
}

func run() error {
	for muldiv() != nil {
		fmt.Printf("\033[2J\033[H")
	}
	fmt.Println("ALL GOOD! (Press enter to quit.)")
	buf := make([]byte, 16)
	os.Stdin.Read(buf)
	return nil
}

func muldiv() error {
	n := 6
	for i := range n {
		a, b, r, kind := 0, rand.IntN(3)+2, 0, rand.IntN(2)
		switch b {
		case 2:
			a = rand.IntN(59) + 2
		case 3:
			a = rand.IntN(12) + 2
		case 4:
			a = rand.IntN(13) + 2
		}
		fmt.Printf("T%d/%d: ", i+1, n)
		if kind == 0 {
			fmt.Printf(" %2d * %d = ", a, b)
		} else {
			fmt.Printf(" %2d / %d = ", a*b, b)
		}
		fmt.Scan(&r)
		if kind == 0 && r != a*b {
			fmt.Printf("WRONG! Correct: %d\n", a*b)
			time.Sleep(4 * time.Second)
			return fmt.Errorf("WA")
		} else if kind == 1 && r != a {
			fmt.Printf("WRONG! Correct: %d\n", a/b)
			time.Sleep(4 * time.Second)
			return fmt.Errorf("WA")
		}
	}
	return nil
}
