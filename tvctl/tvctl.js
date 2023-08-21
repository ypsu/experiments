"use strict";
let currentLevel = 0;
let challenge;
function keydown(evt) {
    if (evt.key == 'F5') {
        evt.preventDefault();
        return;
    }
    if (evt.altKey || evt.ctrlKey)
        return;
    if (challenge.onkeydown) {
        challenge.onkeydown(evt);
        challenge.render();
    }
}
function keyup(evt) {
    if (evt.altKey || evt.ctrlKey)
        return;
    if (challenge.onkeyup) {
        challenge.onkeyup(evt);
        challenge.render();
    }
}
async function reward() {
    if (pass != 'PASS') {
        let r = await fetch('/reward', {
            method: 'POST',
            body: pass,
        });
        if (!r.ok)
            return;
    }
    pass = '';
    hpasscode.innerText = '';
    hchallenge.hidden = true;
    hcorrectmsg.hidden = true;
    window.onkeydown = null;
    currentLevel++;
    setTimeout(() => {
        hchallenge.hidden = false;
        hcorrectmsg.hidden = true;
        if (challenge.onkeydown)
            window.onkeydown = keydown;
        if (challenge.init)
            challenge.init();
        challenge.render();
    }, 2000);
}
let pass = '';
function winstate() {
    hcorrectmsg.hidden = false;
    window.onkeydown = evt => {
        if (evt.altKey || evt.ctrlKey)
            return;
        if (evt.key == 'Enter')
            reward();
        if (evt.key == 'Backspace' && pass.length > 0)
            pass = pass.slice(0, -1);
        if (evt.key.length == 1)
            pass += evt.key.toUpperCase();
        hpasscode.innerText = pass;
    };
}
function main() {
    if (challenge.init)
        challenge.init();
    window.onkeydown = keydown;
    window.onkeyup = evt => {
        if (evt.key == 'F5') {
            evt.preventDefault();
            return;
        }
        if (evt.altKey || evt.ctrlKey)
            return;
        if (challenge.onkeyup) {
            challenge.onkeyup(evt);
            challenge.render();
        }
    };
    challenge.render();
}
class missingnum {
    nums = new Array(9);
    nummask = 0;
    solution = 0;
    pressed = 0;
    solved = 0;
    init() {
        this.pressed = 0;
        this.nummask = 0;
        for (let r = 0; r < 3; r++) {
            for (let c = 0; c < 3; c++) {
                let rnd;
                do {
                    rnd = Math.floor(Math.random() * 9) + 1;
                } while ((this.nummask & 1 << rnd) != 0);
                this.nums[r * 3 + c] = rnd;
                this.nummask |= 1 << rnd;
            }
        }
        this.solution = this.nums[8];
        this.nummask ^= 1 << this.solution;
        this.nums[8] = '?';
    }
    render() {
        let html = '';
        html += `level ${this.solved + 1} / ${currentLevel + 3}<br>`;
        html += '<table>';
        for (let r = 0; r < 3; r++) {
            html += '<tr>';
            for (let c = 0; c < 3; c++) {
                if (r == 2 && c == 2) {
                    if (this.pressed === this.nums[8]) {
                        html += '<td style=background-color:lightgreen>';
                    }
                    else {
                        html += '<td>';
                    }
                }
                else if (this.nums[r * 3 + c] == this.pressed) {
                    html += '<td style=background-color:orange>';
                }
                else {
                    html += '<td>';
                }
                html += `${this.nums[r * 3 + c]}</td>`;
            }
            html += '</tr>';
        }
        html += '</table>';
        hchallenge.innerHTML = html;
    }
    onkeyup(ev) {
        if (this.pressed != 0)
            return;
        this.pressed = parseInt(ev.key);
        if (!(1 <= this.pressed && this.pressed <= 9)) {
            this.pressed = 0;
            return;
        }
        this.nums[8] = this.solution;
        if ((this.nummask & 1 << this.pressed) == 0) {
            if (this.solved + 1 == currentLevel + 3) {
                winstate();
                setTimeout(() => {
                    this.solved = 0;
                }, 100);
            }
            else {
                setTimeout(() => {
                    this.solved++;
                    this.init();
                    this.render();
                }, 1000);
            }
        }
        else {
            // using a large delay to discourage guessing.
            const delay = 3000;
            setTimeout(() => {
                this.solved = 0;
                this.init();
                this.render();
            }, delay);
        }
    }
}
// randint returns an integer in the [1, n] range.
function randint(n) {
    return Math.ceil(Math.random() * n);
}
class add3 {
    failed = false;
    problems = 0;
    solved = 0;
    nums = new Array();
    init() {
        this.failed = false;
        this.problems = 4 + currentLevel;
        this.solved = 0;
        this.nums = [];
        for (let i = 0; i < this.problems; i++) {
            let x, y, z;
            do {
                [x, y, z] = [randint(7), randint(7), randint(7)];
            } while (x + y + z > 9);
            this.nums.push(x, y, z);
        }
    }
    render() {
        let html = '';
        for (let i = 0; i < this.problems; i++) {
            let [x, y, z] = [this.nums[i * 3 + 0], this.nums[i * 3 + 1], this.nums[i * 3 + 2]];
            html += `${x} + ${y} + ${z} = `;
            if (i < this.solved) {
                html += `${x + y + z}\n`;
            }
            else if (i == this.solved) {
                if (this.failed) {
                    html += `<span style=color:red>${x + y + z}</span>\n`;
                }
                else {
                    html += '?\n';
                }
            }
            else {
                html += '\n';
            }
        }
        hchallenge.innerHTML = html;
    }
    onkeyup(ev) {
        if (this.failed)
            return;
        let num = parseInt(ev.key);
        if (!(1 <= num && num <= 9))
            return;
        let expected = this.nums[this.solved * 3] + this.nums[this.solved * 3 + 1] + this.nums[this.solved * 3 + 2];
        if (num != expected) {
            this.failed = true;
            setTimeout(() => {
                this.init();
                this.render();
            }, 2000);
            return;
        }
        this.solved++;
        if (this.solved == this.problems)
            winstate();
    }
}
class sub {
    failed = false;
    problems = 0;
    solved = 0;
    nums = new Array();
    init() {
        this.failed = false;
        this.problems = 4 + currentLevel;
        this.solved = 0;
        this.nums = [];
        for (let i = 0; i < this.problems; i++) {
            let x, y;
            do {
                [x, y] = [randint(9), randint(9)];
            } while (x < y);
            this.nums.push(x, y);
        }
    }
    render() {
        let html = '';
        for (let i = 0; i < this.problems; i++) {
            let [x, y] = [this.nums[i * 2 + 0], this.nums[i * 2 + 1]];
            html += `${x} - ${y} = `;
            if (i < this.solved) {
                html += `${x - y}\n`;
            }
            else if (i == this.solved) {
                if (this.failed) {
                    html += `<span style=color:red>${x - y}</span>\n`;
                }
                else {
                    html += '?\n';
                }
            }
            else {
                html += '\n';
            }
        }
        hchallenge.innerHTML = html;
    }
    onkeyup(ev) {
        if (this.failed)
            return;
        let num = parseInt(ev.key);
        if (!(0 <= num && num <= 9))
            return;
        let expected = this.nums[this.solved * 2] - this.nums[this.solved * 2 + 1];
        if (num != expected) {
            this.failed = true;
            setTimeout(() => {
                this.init();
                this.render();
            }, 2000);
            return;
        }
        this.solved++;
        if (this.solved == this.problems)
            winstate();
    }
}
class expr {
    failed = false;
    problems = 0;
    solved = 0;
    nums = new Array();
    ops = ['-', '+'];
    init() {
        this.failed = false;
        this.problems = 4 + currentLevel;
        this.solved = 0;
        this.nums = [];
        for (let i = 0; i < this.problems; i++) {
            let x, op1, y, op2, z;
            while (true) {
                [x, op1, y, op2, z] = [randint(9), randint(2) - 1, randint(8), randint(2) - 1, randint(9)];
                let s = 0;
                if (op1 == 0)
                    s = x - y;
                if (op1 == 1)
                    s = x + y;
                if (s < 0 || s > 11)
                    s = -1;
                if (s != -1 && op2 == 0)
                    s -= z;
                if (s != -1 && op2 == 1)
                    s += z;
                if (s < 0 || s > 9)
                    s = -1;
                if (s == -1)
                    continue;
                this.nums.push(x, op1, y, op2, z, s);
                break;
            }
        }
    }
    render() {
        let html = '';
        for (let i = 0; i < this.problems; i++) {
            let [x, op1, y, op2, z, a] = [
                this.nums[i * 6 + 0],
                this.nums[i * 6 + 1],
                this.nums[i * 6 + 2],
                this.nums[i * 6 + 3],
                this.nums[i * 6 + 4],
                this.nums[i * 6 + 5],
            ];
            html += `${x} ${this.ops[op1]} ${y} ${this.ops[op2]} ${z} = `;
            if (i < this.solved) {
                html += `${a}\n`;
            }
            else if (i == this.solved) {
                if (this.failed) {
                    html += `<span style=color:red>${a}</span>\n`;
                }
                else {
                    html += '?\n';
                }
            }
            else {
                html += '\n';
            }
        }
        hchallenge.innerHTML = html;
    }
    onkeyup(ev) {
        if (this.failed)
            return;
        let num = parseInt(ev.key);
        if (!(0 <= num && num <= 9))
            return;
        let expected = this.nums[this.solved * 6 + 5];
        if (num != expected) {
            this.failed = true;
            setTimeout(() => {
                this.init();
                this.render();
            }, 2000);
            return;
        }
        this.solved++;
        if (this.solved == this.problems)
            winstate();
    }
}
class typefast {
    words = new Array();
    solved = 0;
    currentOK = 0;
    currentError = '';
    resetTimer = 0;
    init() {
        let n = 4 + currentLevel;
        this.solved = 0;
        this.currentOK = 0;
        this.currentError = '';
        this.words = new Array(n);
        for (let i = 0; i < n; i++) {
            this.words[i] = shortwords[Math.floor(Math.random() * shortwords.length)];
        }
    }
    render() {
        let html = '';
        for (let i = 0; i < this.words.length; i++) {
            if (i < this.solved) {
                html += `  <span style=color:green>${this.words[i]}</span>\n`;
            }
            else if (i == this.solved) {
                html += `> <span style=color:green>${this.words[i].slice(0, this.currentOK)}</span><span style=color:red>${this.currentError}</span>${this.words[i].slice(this.currentOK)}\n`;
            }
            else {
                html += `  ${this.words[i]}\n`;
            }
        }
        hchallenge.innerHTML = html;
    }
    resetword() {
        this.currentOK = 0;
        this.currentError = '';
        this.render();
    }
    onkeyup(ev) {
        if (ev.key.length != 1)
            return;
        if (this.currentError != '' || this.solved == this.words.length)
            return;
        let k = ev.key.toUpperCase();
        clearTimeout(this.resetTimer);
        // player pressed the wrong button.
        if (k != this.words[this.solved][this.currentOK]) {
            this.currentError = k;
            this.resetTimer = setTimeout(() => this.resetword(), 1000);
            return;
        }
        // player pressed the right button.
        this.currentOK++;
        if (this.currentOK == this.words[this.solved].length) {
            this.solved++;
            this.currentOK = 0;
            this.currentError = '';
            if (this.solved == this.words.length)
                winstate();
        }
        else {
            this.resetTimer = setTimeout(() => this.resetword(), 1000);
        }
    }
}
class blindfind {
    r = 50;
    pos = [0, 0];
    oldpos = [0, 0];
    dst = [0, 0];
    down = 0;
    rounds = 3;
    round = 0;
    color = false;
    ctx;
    constructor() {
        hchallenge.innerHTML = '<pre id=hnote></pre><canvas id=hcanvas width=1800 height=700 style="border:1px solid">';
        this.ctx = hcanvas.getContext('2d');
    }
    init() {
        this.pos = [50, 50];
        this.dst = [Math.random() * 1500 + 200, Math.random() * 500 + 100];
        this.round++;
    }
    onkeydown(ev) {
        let olddown = this.down;
        if (ev.code == 'ArrowLeft')
            this.down |= 1;
        if (ev.code == 'ArrowRight')
            this.down |= 2;
        if (ev.code == 'ArrowUp')
            this.down |= 4;
        if (ev.code == 'ArrowDown')
            this.down |= 8;
        if (olddown == 0 && this.down > 0)
            this.simulate();
    }
    onkeyup(ev) {
        if (ev.code == 'ArrowLeft')
            this.down &= ~1;
        if (ev.code == 'ArrowRight')
            this.down &= ~2;
        if (ev.code == 'ArrowUp')
            this.down &= ~4;
        if (ev.code == 'ArrowDown')
            this.down &= ~8;
    }
    render() {
        let oldd = Math.hypot(this.dst[0] - this.oldpos[0], this.dst[1] - this.oldpos[1]);
        let newd = Math.hypot(this.dst[0] - this.pos[0], this.dst[1] - this.pos[1]);
        this.ctx.beginPath();
        this.ctx.arc(this.oldpos[0], this.oldpos[1], this.r + 1, 0, 2 * Math.PI);
        this.ctx.fillStyle = '#fff';
        this.ctx.fill();
        this.oldpos[0] = this.pos[0];
        this.oldpos[1] = this.pos[1];
        this.ctx.beginPath();
        this.ctx.arc(this.pos[0], this.pos[1], this.r, 0, 2 * Math.PI);
        this.ctx.fillStyle = '#000';
        if (this.color) {
            if (newd < oldd)
                this.ctx.fillStyle = '#0f0';
            if (newd > oldd)
                this.ctx.fillStyle = '#f00';
        }
        this.ctx.fill();
        if (hcorrectmsg.hidden) {
            hnote.innerText = `round ${this.round}/${this.rounds}: ${Math.round(Math.hypot(this.dst[0] - this.pos[0], this.dst[1] - this.pos[1]))}`;
        }
        else {
            hnote.innerText = 'all done!';
        }
    }
    simulate() {
        let f = 5;
        if ((this.down & 1) != 0 && this.pos[0] - f >= this.r)
            this.pos[0] -= f;
        if ((this.down & 2) != 0 && this.pos[0] + f <= 1800 - this.r)
            this.pos[0] += f;
        if ((this.down & 4) != 0 && this.pos[1] - f >= this.r)
            this.pos[1] -= f;
        if ((this.down & 8) != 0 && this.pos[1] + f <= 700 - this.r)
            this.pos[1] += f;
        this.render();
        if (Math.hypot(this.dst[0] - this.pos[0], this.dst[1] - this.pos[1]) < 50) {
            if (this.round == this.rounds) {
                this.round = 0;
                winstate();
            }
            else {
                this.init();
            }
            return;
        }
        if (this.down > 0) {
            window.requestAnimationFrame(() => this.simulate());
        }
    }
}
class compare {
    nums = new Array();
    ok = 0;
    failed = false;
    init() {
        let n = 6 + currentLevel;
        this.ok = 0;
        this.failed = false;
        this.nums = new Array();
        while (this.nums.length < 2 * n) {
            this.nums.push(Math.floor(Math.random() * 300));
            let k = this.nums.length - 2;
            if (k >= 0 && this.nums[k] == this.nums[k + 1])
                this.nums.pop();
        }
    }
    render() {
        let html = '';
        for (let i = 0; i < this.nums.length / 2; i++) {
            html += `${this.nums[2 * i]} `;
            if (i == this.ok && !this.failed) {
                html += '?';
            }
            else {
                let color = 'white';
                if (i < this.ok) {
                    color = 'green';
                }
                else if (i == this.ok && this.failed) {
                    color = 'red';
                }
                html += `<span style=color:${color}>`;
                html += this.nums[2 * i] < this.nums[2 * i + 1] ? '<' : '>';
                html += '</span>';
            }
            html += ` ${this.nums[2 * i + 1]}\n`;
        }
        hchallenge.innerHTML = html;
    }
    onkeyup(ev) {
        if (ev.key != '1' && ev.key != '2')
            return;
        if (this.failed)
            return this.init();
        let k = this.ok;
        if ((this.nums[2 * k] < this.nums[2 * k + 1]) == (ev.key == '1')) {
            this.ok++;
        }
        else {
            this.failed = true;
        }
        if (this.ok == this.nums.length / 2)
            winstate();
    }
}
class colorNBack {
    k = 9;
    n = 3;
    solved = 0;
    nums = [0];
    wrong = 0;
    colors = [
        '#000',
        '#22f',
        '#080',
        '#0cc',
        '#f80',
        '#f00',
        '#f0f',
        '#808',
        '#cc0',
    ];
    init() {
        this.k = 9 + currentLevel;
        this.nums = [];
        this.solved = 0;
        this.wrong = 0;
        for (let i = 0; i < this.k; i++)
            this.nums.push(Math.floor(Math.random() * this.colors.length));
    }
    render() {
        let h = '';
        for (let i = 0; i < this.solved; i++) {
            let c = this.colors[this.nums[i]];
            h += `<span style=color:${c}>■</span> `;
        }
        if (this.wrong != 0) {
            let c = this.colors[this.nums[this.solved]];
            h += `<span style=color:${c}>■</span> `;
            for (let i = 1; i < this.n && i + this.solved < this.k; i++)
                h += '_ ';
        }
        else if (this.solved == 0) {
            for (let i = 0; i < this.n; i++) {
                let c = this.colors[this.nums[i]];
                h += `<span style=color:${c}>■</span> `;
            }
        }
        else if (this.solved < this.k) {
            for (let i = 0; i < this.n && i + this.solved < this.k; i++)
                h += '_ ';
        }
        if (this.solved + this.n < this.k) {
            let c = this.colors[this.nums[this.solved + this.n]];
            h += `<span style=color:${c}>■</span> `;
        }
        for (let i = this.solved + this.n + 1; i < this.k; i++)
            h += '_ ';
        if (this.solved < this.k) {
            h += '\n';
            for (let i = 0; i < this.solved; i++)
                h += '  ';
            if (this.wrong != 0) {
                h += 'x';
            }
            else {
                h += '^';
            }
        }
        h += '\n\n';
        for (let i = 0; i < this.colors.length; i++) {
            h += `<span style=color:${this.colors[i]} id=hColor${i}>■</span> `;
        }
        hchallenge.innerHTML = h;
        for (let i = 0; i < this.colors.length; i++) {
            document.getElementById(`hColor${i}`).onclick = () => this.click(i);
        }
    }
    click(num) {
        if (this.nums[this.solved] == num) {
            this.solved++;
            if (this.solved == this.k)
                winstate();
        }
        else if (this.solved > 0) {
            this.wrong = 1;
            setTimeout(() => {
                this.init();
                this.render();
            }, 1000);
        }
        this.render();
    }
}
class wordsearch {
    n = 7;
    round = 0;
    rounds = 3;
    grid = [''];
    dr = [-1, -1, +0, +1, +1, +1, +0, -1];
    dc = [+0, +1, +1, +1, +0, -1, -1, -1];
    clicked = [0];
    init() {
        this.round = 1;
        this.reset();
    }
    reset() {
        this.clicked = [];
        this.grid = new Array(this.n * this.n);
        for (let r = 0; r < this.n; r++) {
            for (let c = 0; c < this.n; c++) {
                this.grid[r * this.n + c] = String.fromCharCode(65 + Math.random() * 26);
            }
        }
        let s, sr, sc, d, e, er, ec;
        do {
            s = Math.floor(Math.random() * this.n * this.n);
            sr = Math.floor(s / this.n);
            sc = s % this.n;
            d = Math.floor(Math.random() * 8);
            er = sr + 3 * this.dr[d];
            ec = sc + 3 * this.dc[d];
            e = er * this.n + ec;
        } while (er < 0 || er >= this.n || ec < 0 || ec >= this.n);
        this.grid[s] = 'A';
        this.grid[s + this.dr[d] * this.n + this.dc[d]] = 'D';
        this.grid[s + 2 * this.dr[d] * this.n + 2 * this.dc[d]] = 'A';
        this.grid[e] = 'M';
    }
    click(id) {
        if (this.clicked.length == 0 && this.grid[id] != 'A')
            return;
        if (this.clicked.length == 0) {
            this.clicked.push(id);
            this.render();
            return;
        }
        if (this.grid[id] != 'ADAM'[this.clicked.length]) {
            this.init();
            this.render();
            return;
        }
        let lid = this.clicked[this.clicked.length - 1];
        if (id == lid)
            return;
        let lr = Math.floor(lid / this.n);
        let lc = lid % this.n;
        let r = Math.floor(id / this.n);
        let c = id % this.n;
        if (Math.abs(r - lr) >= 2 || Math.abs(c - lc) >= 2) {
            this.init();
            this.render();
            return;
        }
        this.clicked.push(id);
        if (this.clicked.length == 4) {
            if (this.round == this.rounds) {
                this.render();
                winstate();
                return;
            }
            this.round++;
            this.reset();
        }
        this.render();
    }
    render() {
        let h = '';
        h += `round ${this.round}/${this.rounds}\n`;
        for (let r = 0; r < this.n; r++) {
            for (let c = 0; c < this.n; c++) {
                let id = r * this.n + c;
                let style = '';
                if (this.clicked.includes(id))
                    style = ' style=color:green';
                h += `<span id=cell${id}${style}>${this.grid[id]}</span> `;
            }
            h += '\n';
        }
        hchallenge.innerHTML = h;
        for (let i = 0; i < this.n * this.n; i++) {
            document.getElementById(`cell${i}`).onclick = () => this.click(i);
        }
    }
}
var tttResult;
(function (tttResult) {
    tttResult[tttResult["unknown"] = 0] = "unknown";
    tttResult[tttResult["lose"] = 1] = "lose";
    tttResult[tttResult["draw"] = 2] = "draw";
    tttResult[tttResult["win"] = 3] = "win";
})(tttResult || (tttResult = {}));
class tictactoe {
    round = 0;
    rounds = 3;
    board = new Array(9);
    status = '';
    winner3(a, b, c) {
        if (a != b || a != c)
            return '';
        return a;
    }
    winner9() {
        let p = '';
        p += this.winner3(this.board[0], this.board[1], this.board[2]);
        p += this.winner3(this.board[3], this.board[4], this.board[5]);
        p += this.winner3(this.board[6], this.board[7], this.board[8]);
        p += this.winner3(this.board[0], this.board[3], this.board[6]);
        p += this.winner3(this.board[1], this.board[4], this.board[7]);
        p += this.winner3(this.board[2], this.board[5], this.board[8]);
        p += this.winner3(this.board[0], this.board[4], this.board[8]);
        p += this.winner3(this.board[2], this.board[4], this.board[6]);
        if (p == '')
            return '';
        return p.slice(0, 1);
    }
    pickRandomly(a) {
        return a[Math.floor(Math.random() * a.length)];
    }
    extractResult(p, n) {
        let w = this.winner9();
        if (w == p)
            return tttResult.win;
        if (w == n)
            return tttResult.lose;
        for (let i = 0; i < 9; i++) {
            if (this.board[i] == '')
                return tttResult.unknown;
        }
        return tttResult.draw;
    }
    computeMove(p, n) {
        let r = this.extractResult(p, n);
        if (r != tttResult.unknown)
            return [r, -1];
        let winMoves = [];
        let drawMoves = [];
        let loseMoves = [];
        for (let i = 0; i < 9; i++) {
            if (this.board[i] != '')
                continue;
            this.board[i] = p;
            let [r, m] = this.computeMove(n, p);
            if (r == tttResult.win)
                loseMoves.push(i);
            if (r == tttResult.draw)
                drawMoves.push(i);
            if (r == tttResult.lose)
                winMoves.push(i);
            this.board[i] = '';
        }
        if (winMoves.length > 0)
            return [tttResult.win, this.pickRandomly(winMoves)];
        if (drawMoves.length > 0)
            return [tttResult.draw, this.pickRandomly(drawMoves)];
        if (loseMoves.length > 0)
            return [tttResult.lose, this.pickRandomly(loseMoves)];
        return [tttResult.unknown, -1];
    }
    init() {
        this.round = 0;
        this.status = '';
        this.reset();
    }
    reset() {
        if (this.status.startsWith('DRAW')) {
            this.round++;
        }
        else {
            this.round = 0;
        }
        this.status = '';
        for (let i = 0; i < 9; i++)
            this.board[i] = '';
        let [r, m] = this.computeMove('O', 'X');
        this.board[m] = 'O';
    }
    render() {
        let h = '';
        if (this.round < this.rounds) {
            h += `round ${this.round + 1}/${this.rounds}:`;
        }
        else {
            h += `all ${this.rounds} rounds done!`;
        }
        h += '<table>\n<tr>';
        h += this.cell(0) + this.cell(1) + this.cell(2) + '\n<tr>';
        h += this.cell(3) + this.cell(4) + this.cell(5) + '\n<tr>';
        h += this.cell(6) + this.cell(7) + this.cell(8) + '\n<tr>';
        h += '</table>\n';
        h += this.status;
        hchallenge.innerHTML = h;
        for (let i = 0; i < 9; i++) {
            document.getElementById(`cell${i}`).onclick = () => this.click(i);
        }
    }
    cell(i) {
        let s = this.board[i];
        if (s == '')
            s = ' ';
        return `<td id=cell${i}>${s}</td>`;
    }
    click(i) {
        if (this.round == this.rounds)
            return;
        if (this.status != '') {
            this.reset();
            this.render();
            return;
        }
        let emptycnt = 0;
        for (let i = 0; i < 9; i++) {
            if (this.board[i] == '')
                emptycnt++;
        }
        if (emptycnt % 2 == 1) {
            // ignore click, wait for the bot to move instead.
            return;
        }
        if (this.board[i] != '')
            return;
        this.board[i] = 'X';
        if (this.extractResult('X', 'O') == tttResult.win)
            this.status = 'X WON! \\o/';
        if (this.extractResult('O', 'X') == tttResult.draw)
            this.status = 'DRAW \\o/';
        this.render();
        setTimeout(() => {
            let [r, m] = this.computeMove('O', 'X');
            this.board[m] = 'O';
            if (this.extractResult('O', 'X') == tttResult.win)
                this.status = 'O WON! /o\\';
            if (this.extractResult('O', 'X') == tttResult.draw)
                this.status = 'DRAW \\o/';
            if (this.status.startsWith('DRAW')) {
                if (this.round == this.rounds - 1) {
                    this.round++;
                    winstate();
                }
            }
            this.render();
        }, 500);
    }
}
challenge = new tictactoe();
main();
