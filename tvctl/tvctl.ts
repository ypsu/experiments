declare var hchallenge: HTMLElement, hcorrectmsg: HTMLElement
declare var hcanvas: HTMLCanvasElement, hnote: HTMLElement
declare var shortwords: Array < string > ;

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
  solved = 0

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
    let html = ''
    html += `level ${this.solved+1} / ${currentLevel+3}<br>`
    html += '<table>'
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
      if (this.solved + 1 == currentLevel + 3) {
        winstate()
        setTimeout(() => {
          this.solved = 0
        }, 100)
      } else {
        setTimeout(() => {
          this.solved++
          this.init()
          this.render()
        }, 1000)
      }
    } else {
      // using a large delay to discourage guessing.
      const delay = 3000
      setTimeout(() => {
        this.solved = 0
        this.init()
        this.render()
      }, delay)
    }
  }
}

// randint returns an integer in the [1, n] range.
function randint(n: number) {
  return Math.ceil(Math.random() * n)
}

class add3 {
  failed = false
  problems = 0
  solved = 0
  nums = new Array < number > ()

  init() {
    this.failed = false
    this.problems = 4 + currentLevel
    this.solved = 0
    this.nums = []
    for (let i = 0; i < this.problems; i++) {
      let x, y, z: number
      do {
        [x, y, z] = [randint(7), randint(7), randint(7)]
      } while (x + y + z > 9)
      this.nums.push(x, y, z)
    }
  }

  render() {
    let html = ''
    for (let i = 0; i < this.problems; i++) {
      let [x, y, z] = [this.nums[i * 3 + 0], this.nums[i * 3 + 1], this.nums[i * 3 + 2]]
      html += `${x} + ${y} + ${z} = `
      if (i < this.solved) {
        html += `${x+y+z}\n`
      } else if (i == this.solved) {
        if (this.failed) {
          html += `<span style=color:red>${x+y+z}</span>\n`
        } else {
          html += '?\n'
        }
      } else {
        html += '\n'
      }
    }
    hchallenge.innerHTML = html
  }

  onkeyup(ev: KeyboardEvent) {
    if (this.failed) return
    let num = parseInt(ev.key)
    if (!(1 <= num && num <= 9)) return
    let expected = this.nums[this.solved * 3] + this.nums[this.solved * 3 + 1] + this.nums[this.solved * 3 + 2]
    if (num != expected) {
      this.failed = true
      setTimeout(() => {
        this.init()
        this.render()
      }, 2000)
      return
    }
    this.solved++
    if (this.solved == this.problems) winstate()
  }
}

class sub {
  failed = false
  problems = 0
  solved = 0
  nums = new Array < number > ()

  init() {
    this.failed = false
    this.problems = 4 + currentLevel
    this.solved = 0
    this.nums = []
    for (let i = 0; i < this.problems; i++) {
      let x, y: number
      do {
        [x, y] = [randint(9), randint(9)]
      } while (x < y)
      this.nums.push(x, y)
    }
  }

  render() {
    let html = ''
    for (let i = 0; i < this.problems; i++) {
      let [x, y] = [this.nums[i * 2 + 0], this.nums[i * 2 + 1]]
      html += `${x} - ${y} = `
      if (i < this.solved) {
        html += `${x-y}\n`
      } else if (i == this.solved) {
        if (this.failed) {
          html += `<span style=color:red>${x-y}</span>\n`
        } else {
          html += '?\n'
        }
      } else {
        html += '\n'
      }
    }
    hchallenge.innerHTML = html
  }

  onkeyup(ev: KeyboardEvent) {
    if (this.failed) return
    let num = parseInt(ev.key)
    if (!(0 <= num && num <= 9)) return
    let expected = this.nums[this.solved * 2] - this.nums[this.solved * 2 + 1]
    if (num != expected) {
      this.failed = true
      setTimeout(() => {
        this.init()
        this.render()
      }, 2000)
      return
    }
    this.solved++
    if (this.solved == this.problems) winstate()
  }
}

class expr {
  failed = false
  problems = 0
  solved = 0
  nums = new Array < number > ()
  ops = ['-', '+']

  init() {
    this.failed = false
    this.problems = 4 + currentLevel
    this.solved = 0
    this.nums = []
    for (let i = 0; i < this.problems; i++) {
      let x, op1, y, op2, z: number
      while (true) {
        [x, op1, y, op2, z] = [randint(9), randint(2) - 1, randint(8), randint(2) - 1, randint(9)]
        let s = 0
        if (op1 == 0) s = x - y
        if (op1 == 1) s = x + y
        if (s < 0 || s > 9) s = -1
        if (s != -1 && op2 == 0) s -= z
        if (s != -1 && op2 == 1) s += z
        if (s < 0 || s > 9) s = -1
        if (s == -1) continue
        this.nums.push(x, op1, y, op2, z, s)
        break
      }
    }
  }

  render() {
    let html = ''
    for (let i = 0; i < this.problems; i++) {
      let [x, op1, y, op2, z, a] = [
        this.nums[i * 6 + 0],
        this.nums[i * 6 + 1],
        this.nums[i * 6 + 2],
        this.nums[i * 6 + 3],
        this.nums[i * 6 + 4],
        this.nums[i * 6 + 5],
      ]
      html += `${x} ${this.ops[op1]} ${y} ${this.ops[op2]} ${z} = `
      if (i < this.solved) {
        html += `${a}\n`
      } else if (i == this.solved) {
        if (this.failed) {
          html += `<span style=color:red>${a}</span>\n`
        } else {
          html += '?\n'
        }
      } else {
        html += '\n'
      }
    }
    hchallenge.innerHTML = html
  }

  onkeyup(ev: KeyboardEvent) {
    if (this.failed) return
    let num = parseInt(ev.key)
    if (!(0 <= num && num <= 9)) return
    let expected = this.nums[this.solved * 6 + 5]
    if (num != expected) {
      this.failed = true
      setTimeout(() => {
        this.init()
        this.render()
      }, 2000)
      return
    }
    this.solved++
    if (this.solved == this.problems) winstate()
  }
}

class typefast {
  words = new Array < string > ()
  solved = 0
  currentOK = 0
  currentError = ''
  resetTimer = 0

  init() {
    let n = 4 + currentLevel
    this.solved = 0
    this.currentOK = 0
    this.currentError = ''
    this.words = new Array < string > (n)
    for (let i = 0; i < n; i++) {
      this.words[i] = shortwords[Math.floor(Math.random() * shortwords.length)]
    }
  }

  render() {
    let html = ''
    for (let i = 0; i < this.words.length; i++) {
      if (i < this.solved) {
        html += `  <span style=color:green>${this.words[i]}</span>\n`
      } else if (i == this.solved) {
        html += `> <span style=color:green>${this.words[i].slice(0, this.currentOK)}</span><span style=color:red>${this.currentError}</span>${this.words[i].slice(this.currentOK)}\n`
      } else {
        html += `  ${this.words[i]}\n`
      }
    }
    hchallenge.innerHTML = html
  }

  resetword() {
    this.currentOK = 0
    this.currentError = ''
    this.render()
  }

  onkeyup(ev: KeyboardEvent) {
    if (ev.key.length != 1) return
    if (this.currentError != '' || this.solved == this.words.length) return
    let k = ev.key.toUpperCase()
    clearTimeout(this.resetTimer)

    // player pressed the wrong button.
    if (k != this.words[this.solved][this.currentOK]) {
      this.currentError = k
      this.resetTimer = setTimeout(() => this.resetword(), 1000)
      return
    }

    // player pressed the right button.
    this.currentOK++
    if (this.currentOK == this.words[this.solved].length) {
      this.solved++
      this.currentOK = 0
      this.currentError = ''
      if (this.solved == this.words.length) winstate()
    } else {
      this.resetTimer = setTimeout(() => this.resetword(), 1000)
    }
  }
}

class blindfind {
  r = 50
  pos = [0, 0]
  oldpos = [0, 0]
  dst = [0, 0]
  down = 0
  rounds = 3
  round = 0
  ctx: CanvasRenderingContext2D

  constructor() {
    hchallenge.innerHTML = '<pre id=hnote></pre><canvas id=hcanvas width=1800 height=700 style="border:1px solid">';
    this.ctx = hcanvas.getContext('2d') as CanvasRenderingContext2D
  }

  init() {
    this.pos = [50, 50]
    this.dst = [Math.random() * 1500 + 200, Math.random() * 500 + 100]
    this.round++
  }

  onkeydown(ev: KeyboardEvent) {
    let olddown = this.down
    if (ev.code == 'ArrowLeft') this.down |= 1
    if (ev.code == 'ArrowRight') this.down |= 2
    if (ev.code == 'ArrowUp') this.down |= 4
    if (ev.code == 'ArrowDown') this.down |= 8
    if (olddown == 0 && this.down > 0) this.simulate()
  }

  onkeyup(ev: KeyboardEvent) {
    if (ev.code == 'ArrowLeft') this.down &= ~1
    if (ev.code == 'ArrowRight') this.down &= ~2
    if (ev.code == 'ArrowUp') this.down &= ~4
    if (ev.code == 'ArrowDown') this.down &= ~8
  }

  render() {
    let oldd = Math.hypot(this.dst[0]-this.oldpos[0], this.dst[1]-this.oldpos[1])
    let newd = Math.hypot(this.dst[0]-this.pos[0], this.dst[1]-this.pos[1])
    this.ctx.beginPath()
    this.ctx.arc(this.oldpos[0], this.oldpos[1], this.r+1, 0, 2 * Math.PI)
    this.ctx.fillStyle = '#fff'
    this.ctx.fill()
    this.oldpos[0] = this.pos[0]
    this.oldpos[1] = this.pos[1]
    this.ctx.beginPath()
    this.ctx.arc(this.pos[0], this.pos[1], this.r, 0, 2 * Math.PI)
    this.ctx.fillStyle = '#000'
    if (newd < oldd) this.ctx.fillStyle = '#0f0'
    if (newd > oldd) this.ctx.fillStyle = '#f00'
    this.ctx.fill()
    if (hcorrectmsg.hidden) {
      hnote.innerText = `round ${this.round}/${this.rounds}: ${Math.round(Math.hypot(this.dst[0]-this.pos[0], this.dst[1]-this.pos[1]))}`
    } else {
      hnote.innerText = 'all done!'
    }
  }

  simulate() {
    let f = 5
    if ((this.down & 1) != 0 && this.pos[0] - f >= this.r) this.pos[0] -= f
    if ((this.down & 2) != 0 && this.pos[0] + f <= 1800 - this.r) this.pos[0] += f
    if ((this.down & 4) != 0 && this.pos[1] - f >= this.r) this.pos[1] -= f
    if ((this.down & 8) != 0 && this.pos[1] + f <= 700 - this.r) this.pos[1] += f
    this.render()
    if (Math.hypot(this.dst[0] - this.pos[0], this.dst[1] - this.pos[1]) < 50) {
      if (this.round == this.rounds) {
        this.round = 0
        winstate()
      } else {
        this.init()
      }
      return
    }
    if (this.down > 0) {
      window.requestAnimationFrame(() => this.simulate());
    }
  }
}

challenge = new blindfind()
main()
