declare var hchallenge: HTMLElement
declare var hcorrectmsg: HTMLElement

let currentLevel = 0

let challenge: {
  init ? : () => void,
  render: () => void,
  onkeydown ? : (ev: KeyboardEvent) => void,
  onkeyup ? : (ev: KeyboardEvent) => void,
}

function keydown(evt: KeyboardEvent) {
  if (evt.key == 'F5') {
    evt.preventDefault()
    return
  }
  if (evt.altKey || evt.ctrlKey) return;
  if (challenge.onkeydown) {
    challenge.onkeydown(evt);
    challenge.render();
  }
}

function keyup(evt: KeyboardEvent) {
  if (evt.altKey || evt.ctrlKey) return;
  if (challenge.onkeyup) {
    challenge.onkeyup(evt);
    challenge.render();
  }
}

function reward() {
  hchallenge.hidden = true
  hcorrectmsg.hidden = true
  window.onkeydown = null
  fetch('/reward', {
    method: 'POST'
  })
  currentLevel++
  setTimeout(() => {
    hchallenge.hidden = false
    hcorrectmsg.hidden = true
    if (challenge.onkeydown) window.onkeydown = keydown
    if (challenge.init) challenge.init()
    challenge.render()
  }, 2000)
}

function winstate() {
  hcorrectmsg.hidden = false;
  window.onkeydown = evt => {
    if (evt.altKey || evt.ctrlKey) return;
    if (evt.key == 'Enter') reward();
  };
}

function main() {
  if (challenge.init) challenge.init();
  window.onkeydown = keydown
  window.onkeyup = evt => {
    if (evt.key == 'F5') {
      evt.preventDefault()
      return
    }
    if (evt.altKey || evt.ctrlKey) return;
    if (challenge.onkeyup) {
      challenge.onkeyup(evt);
      challenge.render();
    }
  };
  challenge.render();
}

class missingnum {
  nums = new Array < number | '?' > (9)
  nummask = 0
  solution = 0
  pressed = 0

  init() {
    this.pressed = 0
    this.nummask = 0
    for (let r = 0; r < 3; r++) {
      for (let c = 0; c < 3; c++) {
        let rnd: number
        do {
          rnd = Math.floor(Math.random() * 9) + 1
        } while ((this.nummask & 1 << rnd) != 0)
        this.nums[r * 3 + c] = rnd
        this.nummask |= 1 << rnd
      }
    }
    this.solution = this.nums[8] as number
    this.nummask ^= 1 << this.solution
    this.nums[8] = '?'
  }

  render() {
    let html = '<table>'
    for (let r = 0; r < 3; r++) {
      html += '<tr>'
      for (let c = 0; c < 3; c++) {
        if (r == 2 && c == 2) {
          if (this.pressed === this.nums[8]) {
            html += '<td style=background-color:lightgreen>'
          } else {
            html += '<td>'
          }
        } else if (this.nums[r * 3 + c] == this.pressed) {
          html += '<td style=background-color:orange>'
        } else {
          html += '<td>'
        }
        html += `${this.nums[r*3+c]}</td>`
      }
      html += '</tr>'
    }
    html += '</table>'
    hchallenge.innerHTML = html
  }

  onkeyup(ev: KeyboardEvent) {
    if (this.pressed != 0) return
    this.pressed = parseInt(ev.key)
    if (!(1 <= this.pressed && this.pressed <= 9)) {
      this.pressed = 0
      return
    }
    this.nums[8] = this.solution
    if ((this.nummask & 1 << this.pressed) == 0) {
      winstate()
    } else {
      // using a large delay to discourage guessing.
      const delay = 5000
      setTimeout(() => {
        this.init()
        this.render()
      }, delay)
    }
  }
}

challenge = new missingnum()
main()
