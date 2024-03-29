currentLevel = 0

function reward() {
  hchallenge.hidden = true
  hcorrectmsg.hidden = true
  window.onkeydown = null
  fetch('/reward', {
    method: 'POST'
  })
  if (challenge.toughen) {
    setTimeout(_ => {
      currentLevel++
      hchallenge.hidden = false
      hcorrectmsg.hidden = true
      if (challenge.onkeydown) window.onkeydown = keydown
      challenge.toughen(currentLevel)
      if (challenge.init) challenge.init()
      challenge.render()
    }, 2000)
  }
}

function winstate() {
  hcorrectmsg.hidden = false;
  window.onkeydown = evt => {
    if (evt.altKey || evt.ctrlKey) return;
    if (evt.key == 'Enter') reward();
  };
}

let typing = {
  target: 'ABCDEFGHIJKLMNOPQRSTUVWXYZ',
  entered: '',

  onkeydown: evt => {
    let k = evt.key.toUpperCase();
    if (k == 'BACKSPACE' && typing.entered.length > 0) {
      typing.entered = typing.entered.slice(0, -1);
      evt.preventDefault();
    }
    if (typing.entered.length < typing.target.length && k.length == 1) {
      typing.entered += k;
      evt.preventDefault();
    }
    if (typing.entered == typing.target) winstate();
  },

  render: _ => {
    let h = '';
    for (let i = 0; i < typing.target.length; i++) {
      h += typing.target[i] + ' ';
    }
    h += '\n';
    for (let i = 0; i < typing.entered.length; i++) {
      if (typing.entered[i] == typing.target[i]) {
        h += '<span class=matchingletter>';
      }
      h += typing.entered[i] + ' ';
      if (typing.entered[i] == typing.target[i]) h += '</span>';
    }
    for (let i = typing.entered.length; i < typing.target.length; i++) {
      h += '_ ';
    }
    hchallenge.innerHTML = h;
  },
};

let starcounting = {
  stars: [],
  guess: [],
  places: 6,
  starlimit: 9,

  init: _ => {
    for (let i = 0; i < starcounting.places - 1; i++) {
      let rnd = Math.floor(Math.random() * starcounting.starlimit) + 1;
      starcounting.stars[i] = rnd;
    }
    let val = Math.floor(Math.random() * 4) + 6;
    starcounting.stars[starcounting.places - 1] = val;
  },

  onkeydown: evt => {
    let sc = starcounting;
    let k = evt.key;
    if (k == 'Backspace' && sc.guess.length > 0) {
      sc.guess = sc.guess.slice(0, -1);
      evt.preventDefault();
    }
    if (sc.guess.length < sc.places && k.length == 1) {
      if ('0' <= k && k <= '9') {
        let num = parseInt(k);
        if (num == sc.stars[sc.guess.length]) {
          sc.guess.push(num);
        } else {
          sc.guess = [];
        }
      }
      evt.preventDefault();
    }
    if (sc.guess.length == sc.stars.length) {
      if (sc.guess.every((v, i) => v == sc.stars[i])) winstate();
    }
  },

  render: _ => {
    let sc = starcounting;
    let h = '';
    for (let r = 0; r < sc.places; r++) {
      for (let c = 0; c < 9; c++) {
        if (c == 5) h += ' ';
        if (c < sc.stars[r]) {
          h += '•';
        } else {
          h += ' ';
        }
      }
      h += ' = ';
      if (r == sc.guess.length) h += '_';
      if (r < sc.guess.length) {
        if (sc.guess[r] == sc.stars[r]) {
          h += `<span class=matchingletter>${sc.guess[r]}</span>`;
        } else {
          h += `${sc.guess[r]}`;
        }
      }
      h += '\n';
    }
    hchallenge.innerHTML = h;
  },
};

let addition = {
  nums: [],
  okcnt: 0,
  cnt: 6,

  init: _ => {
    let g = addition;
    for (let i = 0; i < g.cnt; i++) {
      do {
        g.nums[i] = [
          Math.floor(Math.random() * 5 + 1),
          Math.floor(Math.random() * 5 + 1),
        ];
      } while (g.nums[i][0] + g.nums[i][1] >= 10);
    }
    g.okcnt = 0;
  },

  onkeydown: evt => {
    let g = addition;
    let k = evt.key;
    if (g.okcnt < g.cnt && k.length == 1 && '0' <= k && k <= '9') {
      let n = parseInt(k);
      if (n == g.nums[g.okcnt][0] + g.nums[g.okcnt][1]) {
        g.okcnt++;
      } else {
        g.init();
      }
      evt.preventDefault();
    }
    if (g.okcnt == g.cnt) {
      winstate();
    }
  },

  render: _ => {
    let g = addition;
    let h = '';
    for (let i = 0; i < g.cnt; i++) {
      h += `${g.nums[i][0]} + ${g.nums[i][1]} = `;
      if (i < g.okcnt) {
        h += `<span class=matchingletter>${
            g.nums[i][0] + g.nums[i][1]}</span>\n`;
      } else {
        h += '_\n';
      }
    }
    hchallenge.innerHTML = h;
  },
};

let pairs = {
  rows: 4,
  cols: 4,
  nums: [],
  selectedidx: -1,
  done: [],
  failed: false,

  init: _ => {
    let n = pairs.rows * pairs.cols;
    pairs.selectedidx = -1;
    pairs.failed = false;
    for (let i = 0; i < n; i++) {
      pairs.nums[i] = Math.floor(i / 2) + 1;
      pairs.done[i] = false;
    }
    for (let i = n - 1; i > 0; i--) {
      let j = Math.floor(Math.random() * (i + 1));
      [pairs.nums[i], pairs.nums[j]] = [pairs.nums[j], pairs.nums[i]];
    }
  },

  fail: _ => {
    pairs.failed = true;
    pairs.render();
    setTimeout(_ => {
      pairs.init();
      pairs.render();
    }, 500);
  },

  select: idx => {
    if (pairs.done[idx]) return;
    if (idx == pairs.selectedidx) {
      pairs.selectedidx = -1;
      return pairs.render();
    }
    if (pairs.selectedidx == -1) {
      pairs.selectedidx = idx;
      return pairs.render();
    }
    if (pairs.nums[idx] == pairs.nums[pairs.selectedidx]) {
      pairs.done[pairs.selectedidx] = true;
      pairs.done[idx] = true;
      pairs.selectedidx = -1;
      for (let i = 0; i < pairs.rows * pairs.cols; i++) {
        if (!pairs.done[i]) return pairs.render();
      }
      winstate();
      return pairs.render();
    }
    return pairs.fail();
  },

  render: _ => {
    let h = '';
    h += '<table>\n';
    let idx = 0;
    for (let r = 0; r < pairs.rows; r++) {
      h += '<tr>';
      for (let c = 0; c < pairs.cols; c++, idx++) {
        let style = '';
        if (idx == pairs.selectedidx) {
          style = 'style=background-color:#ff0';
        }
        if (pairs.done[idx]) {
          style = 'style=background-color:#0f0';
        }
        if (pairs.failed) {
          style = 'style=background-color:#f00';
        }
        h +=
          `<td onclick=pairs.select(${idx}) ${style}>${pairs.nums[idx]}</td>`;
      }
      h += '</tr>\n';
    }
    h += '</table>';
    hchallenge.innerHTML = h;
  },
};

let glyphs = {
  m: {
    '🪟': 'ablak',
    '🧠': 'agy',
    '🚪': 'ajtó',
    '💼': 'aktatáska',
    '🍎': 'alma',
    '🩲': 'alsónadrág',
    '🍍': 'ananász',
    '🥑': 'avokádó',
    '🚗': 'autó',
    '🏸': 'badminton',
    '🦉': 'bagoly',
    '🐋': 'bálna',
    '🍌': 'banán',
    '🐸': 'béka',
    '🚲': 'bicikli',
    '👙': 'bikini',
    '⌨️': 'billentyűzet',
    '🧷': 'biztosítótű',
    '🤡': 'bohóc',
    '💣': 'bomba',
    '✉️': 'boríték',
    '🦡': 'borz',
    '🎳': 'bowling',
    '🥊': 'boxkesztyű',
    '🦬': 'bölény',
    '🥦': 'brokkoli',
    '🪃': 'bumeráng',
    '🦨': 'bűzös borz',
    '🚌': 'busz',
    '🦈': 'cápa',
    '🧵': 'cérna',
    '✏️': 'ceruza',
    '🚬': 'cigaretta',
    '🍋': 'citrom',
    '🍬': 'cukorka',
    '🐬': 'delphin',
    '🦇': 'denevér',
    '🍉': 'dinnye',
    '🥁': 'dob',
    '🐁': 'egér',
    '🖱️': 'egér',
    '🦄': 'egyszarvú',
    '🪂': 'ejtőernyő',
    '🐘': 'elefánt',
    '🍓': 'eper',
    '☂️': 'esernyő',
    '🌧️': 'eső',
    '🍨': 'fagylalt',
    '🐺': 'farkas',
    '👖': 'farmer',
    '💉': 'fecskendő',
    '🎧': 'fejhallgató',
    '🪓': 'fejsze',
    '☁️': 'felhő',
    '🦩': 'flamingó',
    '⚽': 'focilabda',
    '🦷': 'fog',
    '🪥': 'fogkefe',
    '⚙️': 'fogaskerék',
    '🦭': 'fóka',
    '🧄': 'fokhagyma',
    '🧶': 'fonál',
    '🥏': 'frizbi',
    '🪚': 'fűrész',
    '🕊️': 'galamb',
    '📎': 'gemkapocs',
    '🎸': 'gitár',
    '🍄': 'gomba',
    '🦍': 'gorilla',
    '🛹': 'gördeszka',
    '🛼': 'görkorcsolya',
    '🧅': 'hagyma',
    '🚢': 'hajó',
    '🐟': 'hal',
    '🍔': 'hamburger',
    '🐜': 'hangya',
    '🎒': 'hátitáska',
    '🦢': 'hattyú',
    '🌨️': 'havazás',
    '🏠': 'ház',
    '🎻': 'hegedű',
    '🚁': 'helikopter',
    '🌕': 'hold',
    '🦫': 'hód',
    '⛄': 'hóember',
    '⏳': 'homokóra',
    '❄️': 'hópehely',
    '⚓': 'horgony',
    '🏨': 'hotel',
    '🌡️': 'hőmérő',
    '🐹': 'hörcsög',
    '🎢': 'hullámvasút',
    '🥩': 'hús',
    '🏫': 'iskola',
    '🧊': 'jég',
    '🐻': 'jegesmedve',
    '🎫': 'jegy',
    '🦆': 'kacsa',
    '💩': 'kaki',
    '🌵': 'kaktusz',
    '🎩': 'kalap',
    '🔨': 'kalapács',
    '📷': 'kamera',
    '🥄': 'kanál',
    '🎄': 'karácsonyfa',
    '🐞': 'katicabogár',
    '🐐': 'kecske',
    '🦘': 'kenguru',
    '🛶': 'kenu',
    '🍞': 'kenyér',
    '🔪': 'kés',
    '🧤': 'kesztyű',
    '🐍': 'kígyó',
    '🧩': 'kirakó',
    '🥝': 'kiwi',
    '🐨': 'koala',
    '🎲': 'kocka',
    '🥥': 'kókusz',
    '💀': 'koponya',
    '⚰️': 'koporsó',
    '🏥': 'kórház',
    '⛸️': 'korcsolya',
    '👑': 'korona',
    '🧺': 'kosár',
    '🏀': 'kosárlabda',
    '🪨': 'kő',
    '🍐': 'körte',
    '🩹': 'kötszer',
    '🐊': 'krokodil',
    '🥔': 'krumpli',
    '🪱': 'kukac',
    '🗝️': 'kulcs',
    '🐕': 'kutya',
    '🦥': 'lajhár',
    '🔦': 'lámpa',
    '🪰': 'légy',
    '🪜': 'létra',
    '🐎': 'ló',
    '🐱': 'macska',
    '🐦': 'madár',
    '👠': 'magassarkú',
    '🧲': 'mágnes',
    '🐒': 'majom',
    '🐖': 'malac',
    '🐻': 'medve',
    '🐝': 'méhecske',
    '🚑': 'mentő',
    '⚖️': 'mérleg',
    '🎤': 'mikrofon',
    '🔬': 'mikroszkóp',
    '🎅': 'mikulás',
    '📱': 'mobiltelefon',
    '🥜': 'mogyoró',
    '🐿️': 'mókus',
    '🦝': 'mosómedve',
    '🚂': 'mozdony',
    '🏍️': 'motorbicikli',
    '🔎': 'nagyító',
    '🌻': 'napraforgó',
    '☀️': 'nap',
    '🕶️': 'napszemüveg',
    '✂️': 'olló',
    '🦧': 'orángután',
    '🦁': 'oroszlán',
    '🛡️': 'pajzs',
    '🥞': 'palacsinta',
    '🌴': 'pálmafa',
    '🐼': 'panda',
    '🌶️': 'paprika',
    '🍅': 'paradicsom',
    '🦚': 'páva',
    '🦜': 'papagáj',
    '👛': 'pénztárca',
    '🥨': 'perec',
    '🦋': 'pillangó',
    '🏓': 'ping-pong',
    '🐧': 'pingvin',
    '🍕': 'pizza',
    '🕷️': 'pók',
    '🕸️': 'pókháló',
    '🐙': 'polip',
    '🦃': 'pulyka',
    '📻': 'rádió',
    '📌': 'rajzszeg',
    '🦀': 'rák',
    '🚀': 'rakéta',
    '🚓': 'rendőr',
    '🥕': 'répa',
    '✈️': 'repülő',
    '🤖': 'robot',
    '🦊': 'róka',
    '🛴': 'roller',
    '🌹': 'rózsa',
    '🧀': 'sajt',
    '🧣': 'sál',
    '🐉': 'sárkány',
    '🦅': 'sas',
    '⛺': 'sátor',
    '🧹': 'seprű',
    '🏜️': 'sivatag',
    '🦂': 'skorpió',
    '🧂': 'só',
    '🍝': 'spaghetti',
    '🧢': 'sültössapka',
    '🦔': 'sündisznó',
    '🚕': 'taxi',
    '🐄': 'tehén',
    '🥛': 'tej',
    '🐢': 'teknős',
    '☎️': 'telefon',
    '🔭': 'teleszkóp',
    '⛪': 'templom',
    '🐪': 'teve',
    '🐅': 'tigris',
    '🥚': 'tojás',
    '🖊️': 'toll',
    '🦽': 'tolószék',
    '🌪️': 'tornádó',
    '🎂': 'torta',
    '🚜': 'traktor',
    '👕': 'trikó',
    '🏆': 'trófea',
    '🎺': 'trombita',
    '🌷': 'tulipán',
    '🔥': 'tűz',
    '🎆': 'tüzijáték',
    '🚒': 'tűzoltó',
    '🥒': 'uborka',
    '🐗': 'vaddisznó',
    '🧈': 'vaj',
    '🏰': 'vár',
    '🏎️': 'versenyautó',
    '🦦': 'vidra',
    '🚋': 'villamos',
    '⛵': 'vitorlás',
    '💧': 'vízcsepp',
    '📏': 'vonalzó',
    '🪣': 'vödör',
    '🌋': 'vulkán',
    '🦓': 'zebra',
    '🧦': 'zokni',
    '🎹': 'zongora',
    '🚿': 'zuhanyzó',
  },
  count: 8,
  sel: [],
  order: [],
  solved: [],
  next: 0,

  init: _ => {
    let g = glyphs;
    let hadletter = {};
    let emojis = Object.keys(g.m);
    g.sel = [];
    g.order = [];
    g.solved = [];
    while (g.sel.length < g.count) {
      let emoji = emojis[Math.floor(Math.random() * emojis.length)];
      let word = g.m[emoji];
      if (hadletter[word[0]]) continue;
      hadletter[word[0]] = true;
      g.order[g.sel.length] = g.sel.length;
      g.sel[g.sel.length] = emoji;
    }
    for (let i = g.count - 1; i > 0; --i) {
      let j = Math.floor(Math.random() * (i + 1));
      [g.order[i], g.order[j]] = [g.order[j], g.order[i]];
    }
  },

  capitalize: s => s[0].toUpperCase() + s.slice(1),

  selectemoji: i => {
    let g = glyphs;
    g.solved[i] = false;
    g.next = i;
    g.render();
  },

  selectword: i => {
    let g = glyphs;
    if (i != g.next) {
      g.init();
      return g.render();
    }
    g.solved[i] = true;
    for (let j = 0; j < g.count; j++) {
      if (g.solved[j]) continue;
      g.next = j;
      return g.render();
    }
    winstate();
    g.render();
  },

  render: _ => {
    let g = glyphs;
    let h = '';
    let span = '<span style=width:10em;display:inline-block>';
    for (let i = 0; i < g.count; i++) {
      let e = g.sel[i];
      let w0 = '';
      if (i == g.next) w0 = '?';
      if (g.solved[i]) w0 = g.capitalize(g.m[e]);
      let w1 = `<a onclick=glyphs.selectword(${g.order[i]})>${
          g.capitalize(g.m[g.sel[g.order[i]]])}</a>`;
      h += `<a onclick=glyphs.selectemoji(${i})>${e}</a> ${span}${w0}</span>${
          w1}\n`;
    }
    hchallenge.innerHTML = h;
  },
};

let circles = {
  n: 20,
  r: 50,
  cx: 50,
  cy: 50,
  ocx: 50,
  ocy: 50,
  level: 0,
  obstacles: [],
  ctx: null,
  collision: null,
  dragging: false,
  won: false,

  init: _ => {
    hchallenge.innerHTML =
      '<canvas id=hcanvas width=1800 height=700 style="border:1px solid">';
    hcanvas.onmousemove = circles.onmousemove
    hcanvas.onmouseup = circles.onmouseup
    hcanvas.onmouseleave = _ => circles.dragging = false
    hcanvas.oncontextmenu = _ => false
    circles.won = false
    let canvas = hcanvas;
    let ctx = canvas.getContext('2d');
    circles.ctx = ctx;
    let r = circles.r;
    circles.obstacles = [];
    let color = '#f00';
    if (circles.level == 1) color = '#f80';
    if (circles.level == 2) color = '#fb0';
    for (let i = 0; i < circles.n; i++) {
      let cx = Math.random() * 1700 + r,
        cy = Math.random() * 600 + r;
      if ((cx >= 1500 && cy >= 500) || (cx < 200 && cy < 200)) {
        i--;
        continue;
      }
      ctx.beginPath();
      ctx.arc(cx, cy, r, 0, 2 * Math.PI);
      ctx.fillStyle = color;
      ctx.fill();
      circles.obstacles.push([cx, cy]);
    }
    ctx.beginPath();
    ctx.arc(r, r, r, 0, 2 * Math.PI);
    ctx.fillStyle = '#000';
    ctx.fill();
    ctx.beginPath();
    ctx.arc(1800 - r, 700 - r, r, 0, 2 * Math.PI);
    ctx.fillStyle = '#0f0';
    ctx.fill();
    circles.cx = 50;
    circles.cy = 50;
    circles.ocx = 50;
    circles.ocy = 50;
    circles.collision = null;
    circles.simulate();
  },

  toughen: _ => {
    circles.level = 0
  },

  onmousemove: evt => {
    if (circles.collision) return
    if (circles.won) return
    if (!circles.dragging) {
      let dx = circles.cx - evt.offsetX
      let dy = circles.cy - evt.offsetY
      if (dx * dx + dy * dy > circles.r * circles.r) return
      circles.dragging = true
    }
    circles.cx = evt.offsetX
    circles.cy = evt.offsetY
    circles.simulate()
  },

  onmouseup: evt => {
    if (!circles.collision) return
    circles.dragging = false
    circles.level = 0
    hmsg.hidden = true
    circles.init()
    evt.preventDefault()
  },

  simulate: _ => {
    const d = 5;
    let won = false;
    if (circles.cx != circles.ocx || circles.cy != circles.ocy) {
      let r = circles.r;
      if (circles.cx < r) circles.cx = r;
      if (circles.cx > 1800 - r) circles.cx = 1800 - r;
      if (circles.cy < r) circles.cy = r;
      if (circles.cy > 700 - r) circles.cy = 700 - r;
      let [x, y] = [circles.cx, circles.cy]
      if (x == 1800 - r && y == 700 - r) {
        won = true;
        circles.won = true
        if (circles.level < 1) {
          circles.dragging = false
          circles.level++;
          circles.init();
        } else {
          winstate();
        }
      }
      r -= 10;
      for (let i = 0; i < circles.n; i++) {
        let o = circles.obstacles[i];
        let dx = o[0] - x,
          dy = o[1] - y;
        if (dx * dx + dy * dy > 4 * r * r) continue;
        circles.collision = [(o[0] + x) / 2, (o[1] + y) / 2];
        hmsg.innerText = 'Failed. Click to retry!'
        hmsg.hidden = false;
        break;
      }
      circles.render();
    }
    if (!won && circles.collision == null) {
      window.requestAnimationFrame(circles.simulate);
    }
  },

  render: _ => {
    if (circles.ocx == circles.cx && circles.ocy == circles.cy) return;
    let ctx = circles.ctx;
    ctx.beginPath();
    ctx.arc(circles.ocx, circles.ocy, circles.r, 0, 2 * Math.PI);
    ctx.fillStyle = '#fff';
    ctx.fill();
    ctx.beginPath();
    ctx.arc(circles.cx, circles.cy, circles.r, 0, 2 * Math.PI);
    ctx.fillStyle = '#000';
    ctx.fill();
    circles.ocx = circles.cx;
    circles.ocy = circles.cy;

    if (circles.collision) {
      ctx.beginPath();
      ctx.moveTo(circles.collision[0], 0);
      ctx.lineTo(circles.collision[0], 700);
      ctx.moveTo(0, circles.collision[1]);
      ctx.lineTo(1800, circles.collision[1]);
      ctx.stroke();
    }
  },
};

function fmt2d(n) {
  return n.toString(10).padStart(2, ' ')
}

let countdown = {
  target: new Date('2022-07-23T12:00Z').valueOf(),

  render: _ => {
    let r = countdown.target - Date.now()
    if (r <= 0) {
      challenge = circles
      return main()
    }
    let h = 'Must wait:\n'
    r = Math.floor(r / 1000);
    h += `${fmt2d(r % 60)} seconds\n`
    r = Math.floor(r / 60)
    h += `${fmt2d(r % 60)} minutes\n`
    r = Math.floor(r / 60)
    h += `${fmt2d(r % 24)} hours\n`
    r = Math.floor(r / 24)
    h += `${fmt2d(r)} days`
    hchallenge.innerHTML = h
  },

  onkeydown: _ => {},
}

let digitmemo = {
  numcount: 5,
  numlength: 3,
  nums: [],
  okcount: 0,

  init: _ => {
    let dm = digitmemo
    dm.okcount = 0
    dm.nums = []
    for (let r = 0; r < dm.numcount; r++) {
      let n = 0
      dm.nums.push([])
      for (let c = 0; c < dm.numlength; c++) {
        dm.nums[r].push(Math.floor(Math.random() * 9) + 1)
      }
    }
  },

  toughen: currentLevel => {
    if (currentLevel == 4) {
      digitmemo.numlength++
    } else if (currentLevel < 7) {
      digitmemo.numcount++
    }
  },

  render: _ => {
    let dm = digitmemo
    let h = ''
    for (let r = 0; r < dm.numcount; r++) {
      let okr = Math.floor(dm.okcount / dm.numlength)
      h += okr == r ? ' → ' : '   '
      for (let c = 0; c < dm.numlength; c++) {
        let okc = dm.okcount % dm.numlength
        h += '<span '
        if (r < okr || (r == okr && c < okc)) {
          h += 'style=color:green'
        }
        h += '>'
        if (r != okr || c < okc || okc == 0) {
          h += `${dm.nums[r][c]}`
        } else {
          h += '.'
        }
        h += '</span>'
      }
      h += '\n'
    }
    hchallenge.innerHTML = h
  },

  onkeyup: evt => {
    let dm = digitmemo
    let k = evt.key
    if (k < '0' || k > '9' || dm.okcount == dm.numcount * dm.numlength) return
    k -= '0'
    let okr = Math.floor(dm.okcount / dm.numlength)
    let okc = dm.okcount % dm.numlength
    if (k != dm.nums[okr][okc]) {
      dm.init()
      return
    }
    dm.okcount++
    if (dm.okcount == dm.numcount * dm.numlength) winstate()
  },
}

let lettermemo = {
  wordcount: 5,
  wordlength: 3,
  words: [],
  okcount: 0,

  init: _ => {
    let lm = lettermemo
    lm.okcount = 0
    lm.words = []
    for (let r = 0; r < lm.wordcount; r++) {
      let n = 0
      lm.words.push([])
      for (let c = 0; c < lm.wordlength; c++) {
        lm.words[r].push(String.fromCharCode(Math.floor(Math.random() * 26) + 65))
      }
    }
  },

  toughen: currentLevel => {
    if (currentLevel == 4) {
      lettermemo.wordlength++
    } else if (currentLevel < 7) {
      lettermemo.wordcount++
    }
  },

  render: _ => {
    let lm = lettermemo
    let h = ''
    for (let r = 0; r < lm.wordcount; r++) {
      let okr = Math.floor(lm.okcount / lm.wordlength)
      h += okr == r ? ' → ' : '   '
      for (let c = 0; c < lm.wordlength; c++) {
        let okc = lm.okcount % lm.wordlength
        h += '<span '
        if (r < okr || (r == okr && c < okc)) {
          h += 'style=color:green'
        }
        h += '>'
        if (r != okr || c < okc || okc == 0) {
          h += `${lm.words[r][c]}`
        } else {
          h += '.'
        }
        h += '</span>'
      }
      h += '\n'
    }
    hchallenge.innerHTML = h
  },

  onkeyup: evt => {
    let lm = lettermemo
    let k = evt.key.toUpperCase()
    if (k.length != 1 || k < 'A' || k > 'Z' || lm.okcount == lm.wordcount * lm.wordlength) return
    let okr = Math.floor(lm.okcount / lm.wordlength)
    let okc = lm.okcount % lm.wordlength
    if (k != lm.words[okr][okc]) {
      lm.init()
      return
    }
    lm.okcount++
    if (lm.okcount == lm.wordcount * lm.wordlength) winstate()
  },
}

let wordsmemo = {
  wordcount: 4,
  wordlist: [
    'ADAG',
    'ADAT',
    'AGY',
    'AHA',
    'AHOL',
    'AJAJ',
    'AJAK',
    'AKAD',
    'AKI',
    'AKNA',
    'ALAK',
    'ALAP',
    'ALIG',
    'ALJA',
    'ALKU',
    'ALMA',
    'ALUL',
    'AMI',
    'ANYA',
    'ANYU',
    'APA',
    'APAD',
    'ARAT',
    'ARC',
    'ARRA',
    'ATOM',
    'ATYA',
    'AULA',
    'BAB',
    'BABA',
    'BAJ',
    'BAL',
    'BANK',
    'BARI',
    'BASA',
    'BEAD',
    'BEGY',
    'BEL',
    'BELE',
    'BENN',
    'BENT',
    'BIKA',
    'BILI',
    'BOCI',
    'BOCS',
    'BOKA',
    'BOLT',
    'BONT',
    'BOR',
    'BORS',
    'BORZ',
    'BOT',
    'BUDI',
    'BULI',
    'BUMM',
    'BUSZ',
    'BUTA',
    'CICA',
    'CICI',
    'CIGI',
    'CIKK',
    'CINK',
    'COCA',
    'COMB',
    'COPF',
    'CUCC',
    'CUKI',
    'CUMI',
    'CSAK',
    'CSAL',
    'CSAP',
    'CSEL',
    'CSEN',
    'CSUK',
    'DAL',
    'DARA',
    'DARU',
    'DECI',
    'DEKA',
    'DOB',
    'DOMB',
    'DUDA',
    'DUG',
    'DUMA',
    'EBBE',
    'ECET',
    'EDZ',
    'EGY',
    'EJHA',
    'EJT',
    'EKE',
    'ELAD',
    'ELEM',
    'ELME',
    'ELV',
    'EME',
    'EMEL',
    'EPER',
    'ERED',
    'ERES',
    'ERRE',
    'ESET',
    'ESIK',
    'ESTE',
    'ESTI',
    'ETET',
    'EVEZ',
    'EZEN',
    'EZER',
    'FAGY',
    'FAJ',
    'FAL',
    'FALU',
    'FARM',
    'FED',
    'FEDD',
    'FEJ',
    'FEJT',
    'FEL',
    'FELE',
    'FENE',
    'FENN',
    'FENT',
    'FEST',
    'FILC',
    'FILM',
    'FING',
    'FIX',
    'FOCI',
    'FOG',
    'FOGY',
    'FOJT',
    'FOK',
    'FOLT',
    'FON',
    'FORR',
    'FOS',
    'FURA',
    'FUT',
    'GAZ',
    'GEBE',
    'GIDA',
    'GOLF',
    'GOMB',
    'GOND',
    'GONG',
    'GUMI',
    'GYEP',
    'GYOM',
    'HAB',
    'HAD',
    'HAGY',
    'HAHA',
    'HAJ',
    'HAJT',
    'HAL',
    'HALK',
    'HALL',
    'HAMU',
    'HANG',
    'HARC',
    'HAS',
    'HASI',
    'HAT',
    'HAVI',
    'HAZA',
    'HEG',
    'HEGY',
    'HEHE',
    'HEJ',
    'HELY',
    'HERE',
    'HESS',
    'HETI',
    'HIBA',
    'HINT',
    'HISZ',
    'HIT',
    'HOGY',
    'HOKI',
    'HOL',
    'HOLD',
    'HOLT',
    'HOPP',
    'HORD',
    'HOVA',
    'HOZ',
    'HULL',
    'IDE',
    'IDEG',
    'IDEI',
    'IDOM',
    'IGE',
    'IGEN',
    'IJED',
    'IKER',
    'IKRA',
    'IKSZ',
    'IMA',
    'INAL',
    'INAS',
    'ING',
    'INGA',
    'INOG',
    'INT',
    'IPAR',
    'IRAM',
    'IRAT',
    'IRKA',
    'ITAL',
    'ITAT',
    'ITT',
    'IVAR',
    'IZOM',
    'JAJ',
    'JAJA',
    'JAVA',
    'JEGY',
    'JEL',
    'JOBB',
    'JOG',
    'JOGI',
    'JUH',
    'JUJ',
    'JUSS',
    'JUT',
    'KAJA',
    'KAKA',
    'KAKI',
    'KAN',
    'KAP',
    'KAPA',
    'KAPU',
    'KAR',
    'KARD',
    'KAS',
    'KECS',
    'KEDD',
    'KEDV',
    'KEFE',
    'KEGY',
    'KEL',
    'KELL',
    'KELT',
    'KEN',
    'KEND',
    'KENU',
    'KERT',
    'KEZD',
    'KIAD',
    'KIES',
    'KINN',
    'KINT',
    'KIS',
    'KLUB',
    'KOCA',
    'KOMA',
    'KOMP',
    'KONG',
    'KOPP',
    'KOR',
    'KOS',
    'KOSZ',
    'KUKA',
    'KUKK',
    'KUPA',
    'KUSS',
    'LAK',
    'LAKK',
    'LANT',
    'LAP',
    'LAZA',
    'LEAD',
    'LEJT',
    'LEL',
    'LENG',
    'LENN',
    'LENT',
    'LEP',
    'LES',
    'LESZ',
    'LETT',
    'LIBA',
    'LIFT',
    'LIGA',
    'LILA',
    'LOM',
    'LOMB',
    'LOP',
    'LYUK',
    'MAG',
    'MAGA',
    'MAI',
    'MAJD',
    'MAKK',
    'MAMA',
    'MAR',
    'MARI',
    'MARS',
    'MATT',
    'MEG',
    'MEGY',
    'MELL',
    'MELY',
    'MENT',
    'MER',
    'MERT',
    'MESE',
    'MEZ',
    'MIND',
    'MINK',
    'MINT',
    'MIRE',
    'MISE',
    'MOHA',
    'MOND',
    'MOS',
    'MOST',
    'MOZI',
    'MUST',
    'NAGY',
    'NAIV',
    'NANA',
    'NAP',
    'NAPI',
    'NEDV',
    'NEJE',
    'NEKI',
    'NEM',
    'NINI',
    'NOHA',
    'NONO',
    'NOS',
    'NYAK',
    'NYAL',
    'NYEL',
    'NYER',
    'NYES',
    'NYIT',
    'NYOM',
    'OBOA',
    'ODA',
    'OKOL',
    'OKOS',
    'OKOZ',
    'OKUL',
    'OLAJ',
    'OLD',
    'OLT',
    'OLY',
    'ONT',
    'ORR',
    'OSON',
    'OSZT',
    'OTT',
    'PAD',
    'PAFF',
    'PANG',
    'PAP',
    'PAPA',
    'PARK',
    'PART',
    'PATA',
    'PECH',
    'PER',
    'PERC',
    'PEST',
    'PETE',
    'PFUJ',
    'PIA',
    'PIAC',
    'PICI',
    'PIHE',
    'PIPA',
    'PIPI',
    'PISI',
    'PITE',
    'POFA',
    'POLC',
    'PONT',
    'POR',
    'PORC',
    'PSSZ',
    'PSZT',
    'PUHA',
    'PULT',
    'RAB',
    'RAG',
    'RAGU',
    'RAJ',
    'RAJT',
    'RAJZ',
    'RAK',
    'RANG',
    'REJT',
    'REND',
    'RENG',
    'REST',
    'RIAD',
    'RIZS',
    'ROJT',
    'ROM',
    'ROMA',
    'RONT',
    'ROZS',
    'RUHA',
    'RUM',
    'SAJG',
    'SAJT',
    'SAKK',
    'SARJ',
    'SARK',
    'SAS',
    'SATU',
    'SAV',
    'SEB',
    'SEGG',
    'SEJK',
    'SEJT',
    'SEM',
    'SICC',
    'SIET',
    'SIMA',
    'SOHA',
    'SOK',
    'SOKK',
    'SOR',
    'SORS',
    'SOSE',
    'SZAB',
    'SZAG',
    'SZAK',
    'SZAR',
    'SZED',
    'SZEG',
    'SZEL',
    'SZEM',
    'SZER',
    'SZIA',
    'SZID',
    'SZOK',
    'SZOP',
    'TABU',
    'TAG',
    'TALP',
    'TAN',
    'TANK',
    'TAPS',
    'TART',
    'TATA',
    'TAXI',
    'TEA',
    'TEJ',
    'TEKE',
    'TELE',
    'TELI',
    'TELT',
    'TERV',
    'TEST',
    'TESZ',
    'TETT',
    'TEVE',
    'TIED',
    'TIPP',
    'TOKA',
    'TOL',
    'TOLD',
    'TOLL',
    'TORZ',
    'TUD',
    'TUJA',
    'TUSA',
    'UGAT',
    'UGOR',
    'UGYE',
    'UJJ',
    'ULTI',
    'URAL',
    'URNA',
    'UTAL',
    'UTAS',
    'UTCA',
    'VAD',
    'VAGY',
    'VAJ',
    'VAK',
    'VALL',
    'VAN',
    'VAR',
    'VARR',
    'VAS',
    'VEJE',
    'VELE',
    'VER',
    'VERS',
    'VERT',
    'VESE',
    'VESZ',
    'VET',
    'VICC',
    'VISZ',
    'VITA',
    'VIZI',
    'VOKS',
    'VOLT',
    'VONT',
    'VONZ',
    'ZAB',
    'ZACC',
    'ZAJ',
    'ZEKE',
    'ZENE',
    'ZENG',
    'ZORD',
    'ZSEB',
  ],
  selection: [],
  curword: 0,
  nextchar: 0,

  init: _ => {
    let wm = wordsmemo
    wm.curword = 0
    wm.nextchar = 0
    wm.selection = []
    for (let i = 0; i < wm.wordcount; i++) {
      let ok = false
      let rnd
      while (!ok) {
        rnd = Math.floor(Math.random() * wm.wordlist.length) + 1;
        ok = true
        for (let j = 0; ok && j < i; j++) ok = wm.selection[j] != wm.wordlist[rnd]
      }
      wm.selection.push(wm.wordlist[rnd])
    }
  },

  toughen: _ => {
    if (wordsmemo.wordcount < 7) wordsmemo.wordcount++
  },

  onkeyup: evt => {
    let wm = wordsmemo
    let k = evt.key.toUpperCase()
    if (k.length != 1 || k < 'A' || k > 'Z' || wm.curword == wm.selection.length) return
    if (k != wm.selection[wm.curword][wm.nextchar]) {
      wm.init()
      return
    }
    wm.nextchar++
    if (wm.nextchar == wm.selection[wm.curword].length) {
      wm.nextchar = 0
      wm.curword++
    }
    if (wm.curword == wm.selection.length) winstate()
  },

  render: _ => {
    let wm = wordsmemo
    let h = ''
    for (let r = 0; r < wm.selection.length; r++) {
      h += r == wm.curword ? ' → ' : '   '
      for (let c = 0; c < wm.selection[r].length; c++) {
        h += '<span '
        if (r < wm.curword || (r == wm.curword && c < wm.nextchar)) {
          h += 'style=color:green'
        }
        h += '>'
        if (r != wm.curword || c < wm.nextchar || wm.nextchar == 0) {
          h += `${wm.selection[r][c]}`
        } else {
          h += '.'
        }
        h += '</span>'
      }
      h += '\n'
    }
    hchallenge.innerHTML = h
  },
}

let dircircles = {
  n: 20,
  r: 50,
  cx: 50,
  cy: 50,
  cangle: 0,
  v: 0,
  ocx: 50,
  ocy: 50,
  oangle: 0,
  level: 0,
  obstacles: [],
  ctx: null,
  collision: null,
  // currenty held down keys:
  up: false,
  down: false,
  left: false,
  right: false,

  init: _ => {
    hchallenge.innerHTML =
      '<canvas id=hcanvas width=1800 height=700 style="border:1px solid">';
    let canvas = hcanvas;
    let ctx = canvas.getContext('2d');
    dircircles.ctx = ctx;
    let r = dircircles.r;
    dircircles.obstacles = [];
    let color = '#f00';
    if (dircircles.level == 1) color = '#f80';
    if (dircircles.level == 2) color = '#fb0';
    for (let i = 0; i < dircircles.n; i++) {
      let cx = Math.random() * 1700 + r,
        cy = Math.random() * 600 + r;
      if ((cx >= 1500 && cy >= 500) || (cx < 200 && cy < 200)) {
        i--;
        continue;
      }
      ctx.beginPath();
      ctx.arc(cx, cy, r, 0, 2 * Math.PI);
      ctx.fillStyle = color;
      ctx.fill();
      dircircles.obstacles.push([cx, cy]);
    }
    ctx.beginPath();
    ctx.arc(1800 - r, 700 - r, r, 0, 2 * Math.PI);
    ctx.fillStyle = '#0f0';
    ctx.fill();
    dircircles.v = 0;
    dircircles.cx = 50;
    dircircles.cy = 50;
    dircircles.cangle = 0;
    dircircles.ocx = 50;
    dircircles.ocy = 50;
    dircircles.oangle = 0;
    dircircles.collision = null;
    dircircles.render(true)
    dircircles.simulate();
  },

  toughen: _ => {
    dircircles.level = 0
  },

  onkeydown: evt => {
    if (evt.code == 'ArrowLeft') dircircles.left = true;
    if (evt.code == 'ArrowRight') dircircles.right = true;
    if (evt.code == 'ArrowUp') dircircles.up = true;
    if (evt.code == 'ArrowDown') dircircles.down = true;
    if (evt.code == 'Enter' && dircircles.collision != null) {
      hmsg.hidden = true;
      dircircles.level = 0;
      dircircles.init();
    }
  },

  onkeyup: evt => {
    if (evt.code == 'ArrowLeft') dircircles.left = false;
    if (evt.code == 'ArrowRight') dircircles.right = false;
    if (evt.code == 'ArrowUp') dircircles.up = false;
    if (evt.code == 'ArrowDown') dircircles.down = false;
  },

  simulate: _ => {
    const d = 5;
    let won = false;
    if (dircircles.up || dircircles.down || dircircles.left || dircircles.right || dircircles.v > 0) {
      let r = dircircles.r;
      if (dircircles.left) dircircles.cangle -= 0.05
      if (dircircles.right) dircircles.cangle += 0.05
      if (dircircles.up) dircircles.v += 0.05
      if (dircircles.down) {
        dircircles.v -= 0.05
        if (dircircles.v < 0) dircircles.v = 0
      }
      dircircles.cx += dircircles.v * Math.cos(dircircles.cangle)
      dircircles.cy += dircircles.v * Math.sin(dircircles.cangle)
      if (dircircles.cx < r) dircircles.cx = r;
      if (dircircles.cx > 1800 - r) dircircles.cx = 1800 - r;
      if (dircircles.cy < r) dircircles.cy = r;
      if (dircircles.cy > 700 - r) dircircles.cy = 700 - r;
      let x = dircircles.cx
      let y = dircircles.cy
      if (x == 1800 - r && y == 700 - r) {
        won = true;
        if (dircircles.level < 1) {
          dircircles.level++;
          dircircles.init();
        } else {
          winstate();
        }
      }
      r -= 10;
      for (let i = 0; i < dircircles.n; i++) {
        let o = dircircles.obstacles[i];
        let dx = o[0] - x
        let dy = o[1] - y
        if (dx * dx + dy * dy > 4 * r * r) continue;
        dircircles.collision = [(o[0] + x) / 2, (o[1] + y) / 2];
        hmsg.hidden = false;
        break;
      }
      dircircles.render();
    }
    if (!won && dircircles.collision == null) {
      window.requestAnimationFrame(dircircles.simulate);
    }
  },

  render: force => {
    let samex = dircircles.ocx == dircircles.cx
    let samey = dircircles.ocy == dircircles.cy
    let sameangle = dircircles.oangle == dircircles.cangle
    if (!force && samex && samey && sameangle) return;
    let ctx = dircircles.ctx;
    ctx.beginPath();
    ctx.arc(dircircles.ocx, dircircles.ocy, dircircles.r, 0, 2 * Math.PI);
    ctx.fillStyle = '#fff';
    ctx.fill();
    ctx.beginPath();
    ctx.arc(dircircles.cx, dircircles.cy, dircircles.r, 0, 2 * Math.PI);
    ctx.fillStyle = '#000';
    ctx.fill();
    ctx.beginPath();
    ctx.moveTo(dircircles.cx, dircircles.cy)
    ctx.arc(dircircles.cx, dircircles.cy, dircircles.r, dircircles.cangle - 0.1, dircircles.cangle + 0.1);
    ctx.fillStyle = '#fff';
    ctx.fill();
    dircircles.ocx = dircircles.cx;
    dircircles.ocy = dircircles.cy;
    dircircles.oangle = dircircles.cangle;

    if (dircircles.collision) {
      ctx.beginPath();
      ctx.moveTo(dircircles.collision[0], 0);
      ctx.lineTo(dircircles.collision[0], 700);
      ctx.moveTo(0, dircircles.collision[1]);
      ctx.lineTo(1800, dircircles.collision[1]);
      ctx.stroke();
    }
  },
};

let nback = {
  k: 9,
  n: 3,
  solved: 0,
  wrong: 0,
  nums: [],

  toughen: _ => {
    if (nback.k < 20) nback.k++
  },

  init: _ => {
    nback.solved = 0
    nback.nums = []
    nback.wrong = 0
    for (let i = 0; i < nback.k; i++) nback.nums.push(Math.floor(Math.random() * 9) + 1)
  },

  onkeydown: evt => {
    if (evt.key < '1' || '9' < evt.key) return
    let num = parseInt(evt.key)
    if (nback.nums[nback.solved] == num) {
      nback.solved++
      if (nback.solved == nback.k) winstate()
    } else {
      nback.wrong = num
      setTimeout(_ => {
        nback.init()
        nback.render()
      }, 1000)
    }
  },

  render: _ => {
    let h = ''
    for (let i = 0; i < nback.solved; i++) h += `${nback.nums[i]} `
    if (nback.wrong != 0) {
      h += `<span style=color:red>${nback.wrong}</span><span style=color:green>${nback.nums[nback.solved]}</span>`
      for (let i = 1; i < nback.n && i + nback.solved < nback.k; i++) h += '_ '
    } else if (nback.solved == 0) {
      for (let i = 0; i < nback.n; i++) h += `${nback.nums[i]} `
    } else if (nback.solved < nback.k) {
      for (let i = 0; i < nback.n && i + nback.solved < nback.k; i++) h += '_ '
    }
    if (nback.solved + nback.n < nback.k) h += `${nback.nums[nback.solved+nback.n]} `
    for (let i = nback.solved + nback.n + 1; i < nback.k; i++) h += '_ '
    if (nback.solved < nback.k) {
      h += '\n'
      for (let i = 0; i < nback.solved; i++) h += '  '
      h += '^'
    }
    hchallenge.innerHTML = h
  },
}

let words5 = [
  'ABBAN',
  'ABLAK',
  'ABRAK',
  'ADDIG',
  'ADOMA',
  'ADOTT',
  'AFRIK',
  'AGGAT',
  'AGYAG',
  'AGYAL',
  'AGYAR',
  'AGYON',
  'AHAJT',
  'AHHOZ',
  'AHOGY',
  'AHOVA',
  'AJJAJ',
  'AJNYE',
  'AKKOR',
  'AKONA',
  'AKTUS',
  'ALAKI',
  'ALANT',
  'ALANY',
  'ALARM',
  'ALATT',
  'ALBUM',
  'ALFAJ',
  'ALHAS',
  'ALIBI',
  'ALJAS',
  'ALJAZ',
  'ALKAR',
  'ALKAT',
  'ALKOT',
  'ALMOZ',
  'ALNEM',
  'ALTAT',
  'ALUDT',
  'ALULI',
  'ALVAD',
  'ALZAT',
  'AMELY',
  'AMICE',
  'AMINT',
  'AMIRE',
  'AMODA',
  'AMORF',
  'AMOTT',
  'AMPER',
  'ANGIN',
  'ANGOL',
  'ANNAK',
  'ANTIK',
  'ANTUL',
  'ANYAG',
  'ANYAI',
  'ANNYI',
  'ANYUS',
  'AORTA',
  'APACS',
  'APUCI',
  'APUKA',
  'ARABS',
  'ARADI',
  'ARANY',
  'ARASZ',
  'AROMA',
  'ASZAL',
  'ASZAT',
  'ASZIK',
  'ATTAK',
  'ATYAI',
  'ATYUS',
  'AVAGY',
  'AVULT',
  'AVVAL',
  'AZNAP',
  'AZZAL',
  'BABOS',
  'BADAR',
  'BAGOZ',
  'BAJOR',
  'BAJOS',
  'BALEK',
  'BALGA',
  'BALHA',
  'BALOG',
  'BALOS',
  'BALTA',
  'BAMBA',
  'BANDA',
  'BANGA',
  'BANKA',
  'BANYA',
  'BARKA',
  'BARNA',
  'BAROM',
  'BASZK',
  'BATIK',
  'BATKA',
  'BATYU',
  'BEDOB',
  'BEDUG',
  'BEFAL',
  'BEFED',
  'BEFEN',
  'BEFOG',
  'BEFON',
  'BEFUT',
  'BEHAT',
  'BEHOZ',
  'BEINT',
  'BEJUT',
  'BEKAP',
  'BEKEN',
  'BELEP',
  'BELES',
  'BELGA',
  'BELOP',
  'BENNE',
  'BENTI',
  'BEOLT',
  'BERAK',
  'BEREK',
  'BESTE',
  'BESSZ',
  'BETEG',
  'BETOL',
  'BETON',
  'BETUD',
  'BEVER',
  'BEVET',
  'BEVON',
  'BIBIS',
  'BICEG',
  'BIRGE',
  'BIRKA',
  'BIROK',
  'BLAMA',
  'BLOKK',
  'BODON',
  'BODOR',
  'BODRI',
  'BODZA',
  'BOGOS',
  'BOGOZ',
  'BOGSA',
  'BOKOR',
  'BOKSA',
  'BOKSZ',
  'BOLHA',
  'BOLTI',
  'BOLYH',
  'BOMBA',
  'BOMOL',
  'BONTA',
  'BORDA',
  'BOROS',
  'BOROZ',
  'BORUL',
  'BOTOL',
  'BOTOR',
  'BOTOS',
  'BOTOZ',
  'BRANS',
  'BRONZ',
  'BROSS',
  'BUCKA',
  'BUFOG',
  'BUGGY',
  'BUGYI',
  'BUKIK',
  'BUKSI',
  'BUKTA',
  'BUKTI',
  'BULLA',
  'BUMLI',
  'BUNDA',
  'BUROK',
  'BUTIK',
  'BUTUL',
  'BUTUS',
  'BUZOG',
  'CAFAT',
  'CAFRA',
  'CANGA',
  'CASSA',
  'CEFET',
  'CEFRE',
  'CELLA',
  'CENTI',
  'CEPEL',
  'CETLI',
  'CHAOS',
  'CIBAK',
  'CICIC',
  'CICIZ',
  'CICUS',
  'CIFRA',
  'CIMET',
  'CIMPA',
  'CINEZ',
  'CINKE',
  'CIPEL',
  'CIRKA',
  'CIROK',
  'CIVIL',
  'COBRA',
  'CONTO',
  'CRAWL',
  'CREOL',
  'CUCLI',
  'CUDAR',
  'CUKOR',
  'CUPOG',
  'CSALI',
  'CSAPA',
  'CSATA',
  'CSATT',
  'CSECS',
  'CSEGE',
  'CSEKK',
  'CSEND',
  'CSENG',
  'CSEPP',
  'CSERE',
  'CSESZ',
  'CSIBA',
  'CSIBE',
  'CSIGA',
  'CSIKK',
  'CSIPA',
  'CSIRA',
  'CSITT',
  'CSODA',
  'CSOKI',
  'CSONK',
  'CSONT',
  'CSOZE',
  'CSUDA',
  'CSUHA',
  'CSUKA',
  'CSUMA',
  'CSUPA',
  'CSUTA',
  'DACOL',
  'DACOS',
  'DADOG',
  'DAGAD',
  'DAJKA',
  'DAJNA',
  'DAKLI',
  'DALIA',
  'DALOL',
  'DALOS',
  'DANCS',
  'DANDY',
  'DANOL',
  'DARAB',
  'DARUS',
  'DAUER',
  'DELEJ',
  'DELEL',
  'DELTA',
  'DENDI',
  'DERCE',
  'DERES',
  'DEVLA',
  'DILIS',
  'DIVAT',
  'DOBAJ',
  'DOBOG',
  'DOBOL',
  'DOBOS',
  'DOBOZ',
  'DOGMA',
  'DOHOG',
  'DOHOS',
  'DOLOG',
  'DONGA',
  'DONOG',
  'DOSZT',
  'DRAPP',
  'DRILL',
  'DRUKK',
  'DUDOR',
  'DUDVA',
  'DUETT',
  'DUFLA',
  'DUGIG',
  'DUGUL',
  'DUGVA',
  'DUHAJ',
  'DUHOG',
  'DUNDI',
  'DUNNA',
  'DUPLA',
  'DURVA',
  'DUSKA',
  'DUTYI',
  'DUZMA',
  'DZSEM',
  'EBBEN',
  'EBFOG',
  'EBTEJ',
  'ECSEL',
  'ECSET',
  'EDDIG',
  'EGRES',
  'EGYBE',
  'EGYED',
  'EGYEL',
  'EGYES',
  'EGYEZ',
  'EGYIK',
  'EGYKE',
  'EGYRE',
  'EHEJT',
  'EHHEZ',
  'EJNYE',
  'EKKOR',
  'ELDOB',
  'ELDUG',
  'ELEGY',
  'ELEJE',
  'ELEJT',
  'ELEMI',
  'ELEVE',
  'ELFED',
  'ELFEN',
  'ELFOG',
  'ELFUT',
  'ELHAL',
  'ELHAT',
  'ELHOZ',
  'ELIBE',
  'ELIRT',
  'ELJUT',
  'ELKAP',
  'ELKEL',
  'ELKEN',
  'ELLEN',
  'ELLEP',
  'ELLES',
  'ELLIK',
  'ELLOP',
  'ELMAR',
  'ELMOS',
  'ELOLD',
  'ELOLT',
  'ELRAK',
  'ELTEL',
  'ELTOL',
  'ELVAN',
  'ELVER',
  'ELVET',
  'ELVON',
  'EMAIL',
  'EMBER',
  'EMELT',
  'EMIDE',
  'EMITT',
  'ENGED',
  'ENGEM',
  'ENNEK',
  'ENNEN',
  'ENZIM',
  'ENYHE',
  'ENYIM',
  'ENNYI',
  'EOZIN',
  'EPIKA',
  'EPOSZ',
  'EPRES',
  'ERDEI',
  'EREDJ',
  'ERESZ',
  'ERGYE',
  'ERIDJ',
  'ERJED',
  'ESENG',
  'ESETT',
  'ESKET',
  'ESTVE',
  'ESZER',
  'ESZES',
  'ESZIK',
  'ESZME',
  'ETAPP',
  'ETIKA',
  'EVVEL',
  'EXLEX',
  'EXTRA',
  'EZRED',
  'EZRES',
  'EZZEL',
  'FAARC',
  'FAEKE',
  'FAJTA',
  'FAJUL',
  'FAKAD',
  'FAKUL',
  'FALAP',
  'FALAS',
  'FALAT',
  'FALAZ',
  'FALKA',
  'FALUZ',
  'FAPAD',
  'FARAG',
  'FARBA',
  'FAROK',
  'FAROL',
  'FASOR',
  'FASZA',
  'FASZI',
  'FAUNA',
  'FAZOK',
  'FAZON',
  'FEDEZ',
  'FEJEL',
  'FEJES',
  'FEJFA',
  'FEKTE',
  'FELAD',
  'FELED',
  'FELEL',
  'FELES',
  'FELEZ',
  'FENTI',
  'FERDE',
  'FESEL',
  'FICAM',
  'FICCS',
  'FINAK',
  'FINIS',
  'FINOM',
  'FINTA',
  'FIOLA',
  'FIRKA',
  'FIRMA',
  'FITOS',
  'FITTY',
  'FIZET',
  'FJORD',
  'FLANC',
  'FLEKK',
  'FLOTT',
  'FLUOR',
  'FODOR',
  'FOGAD',
  'FOGAN',
  'FOGAS',
  'FOGAT',
  'FOGAZ',
  'FOGDA',
  'FOGVA',
  'FOKOL',
  'FOKOS',
  'FOKOZ',
  'FONAT',
  'FORDA',
  'FORMA',
  'FOROG',
  'FORSZ',
  'FORTE',
  'FOSIK',
  'FOSOS',
  'FOSZT',
  'FOTEL',
  'FRAKK',
  'FRANC',
  'FRANK',
  'FRIGY',
  'FRISS',
  'FRONT',
  'FUCCS',
  'FUKAR',
  'FURAT',
  'FUSER',
  'FUTAM',
  'FUTOS',
  'FUTTA',
  'FUVAR',
  'FUVAT',
  'GALLY',
  'GAMMA',
  'GANAJ',
  'GARAS',
  'GARAT',
  'GARDA',
  'GATYA',
  'GAZDA',
  'GAZOL',
  'GAZOS',
  'GEBED',
  'GEMMA',
  'GENIE',
  'GENRE',
  'GENUS',
  'GENNY',
  'GERLE',
  'GESZT',
  'GIBIC',
  'GICCS',
  'GIPSZ',
  'GOMBA',
  'GRAMM',
  'GRIFF',
  'GRUND',
  'GRUPP',
  'GSEFT',
  'GUGYI',
  'GULYA',
  'GUMIS',
  'GUMIZ',
  'GURUL',
  'GUVAT',
  'GYALU',
  'GYAUR',
  'GYERE',
  'GYORS',
  'GYUFA',
  'GYULA',
  'HABAR',
  'HABDA',
  'HABOG',
  'HABOS',
  'HADAR',
  'HADFI',
  'HAJAJ',
  'HAJAS',
  'HAJAZ',
  'HAJOL',
  'HAJSZ',
  'HALAD',
  'HALAS',
  'HALMA',
  'HALOM',
  'HALVA',
  'HAMAR',
  'HAMIS',
  'HANEM',
  'HANGA',
  'HAPCI',
  'HARAG',
  'HARAP',
  'HARCI',
  'HARIS',
  'HASAD',
  'HASAL',
  'HASAS',
  'HASIS',
  'HATOD',
  'HATOL',
  'HATOS',
  'HAVAS',
  'HAVER',
  'HAZAI',
  'HAZUG',
  'HEBEG',
  'HEGED',
  'HEGYI',
  'HELYI',
  'HENNA',
  'HENYE',
  'HERMA',
  'HETED',
  'HETEL',
  'HETES',
  'HEVER',
  'HEVES',
  'HIDAS',
  'HIDEG',
  'HIDRA',
  'HIENC',
  'HIMBA',
  'HINDU',
  'HINTA',
  'HITEL',
  'HITES',
  'HJAJA',
  'HOLLA',
  'HOLMI',
  'HOLTA',
  'HOMOK',
  'HONFI',
  'HONOL',
  'HONOS',
  'HOPLA',
  'HORDA',
  'HOROG',
  'HOSSZ',
  'HOTEL',
  'HOZAM',
  'HOZAT',
  'HUHOG',
  'HUJJA',
  'HULLA',
  'HUMOR',
  'HURKA',
  'HUROK',
  'HURUT',
  'HUZAL',
  'HUZAM',
  'HUZAT',
  'IAFIA',
  'IBRIK',
  'IDEAD',
  'IDEKI',
  'IDILL',
  'IFJUL',
  'IGAZI',
  'IGRIC',
  'IHLET',
  'IHLIK',
  'IJEDT',
  'IKLAT',
  'IKTAT',
  'ILDOM',
  'ILLAN',
  'ILLAT',
  'ILLEM',
  'ILLET',
  'ILLIK',
  'ILYEN',
  'ILYES',
  'IMITT',
  'INDEX',
  'INDOK',
  'INDUL',
  'INDUS',
  'INGAT',
  'INGER',
  'INNEN',
  'INTIM',
  'IPARI',
  'IRDAL',
  'IRIGY',
  'IRODA',
  'ISMER',
  'ISTEN',
  'ISZAP',
  'ISZEN',
  'ISZIK',
  'ITTAS',
  'ITTEN',
  'ITYEG',
  'IZGAT',
  'IZGUL',
  'IZMOS',
  'IZMUS',
  'IZZAD',
  'IZZIK',
  'JACHT',
  'JAJOS',
  'JASSZ',
  'JAVAK',
  'JAVAS',
  'JAVUL',
  'JEGEC',
  'JEGEL',
  'JEGES',
  'JELEL',
  'JELEN',
  'JELES',
  'JELEZ',
  'JENKI',
  'JERKE',
  'JOGAR',
  'JOGOS',
  'JOTTA',
  'JUHAR',
  'JUJUJ',
  'JUSZT',
  'KABAR',
  'KABIN',
  'KACAG',
  'KACAJ',
  'KACAT',
  'KACOR',
  'KACSA',
  'KACCS',
  'KADAR',
  'KADET',
  'KAHOL',
  'KAJAK',
  'KAJLA',
  'KAKAS',
  'KALAP',
  'KALIT',
  'KAMAT',
  'KAMRA',
  'KANCA',
  'KANDI',
  'KANNA',
  'KANTA',
  'KAPAR',
  'KAPAT',
  'KAPCA',
  'KAPOR',
  'KAPTA',
  'KAPUS',
  'KAPUT',
  'KARAJ',
  'KARFA',
  'KAROL',
  'KAROM',
  'KAROS',
  'KASKA',
  'KASOS',
  'KASZA',
  'KASZT',
  'KATAT',
  'KAVAR',
  'KAZAL',
  'KEBEL',
  'KECEL',
  'KEHEL',
  'KEHES',
  'KEKSZ',
  'KELEP',
  'KELET',
  'KELME',
  'KELTA',
  'KELTE',
  'KENAF',
  'KENCE',
  'KENET',
  'KEREK',
  'KEREP',
  'KERES',
  'KERET',
  'KERGE',
  'KERTI',
  'KERUB',
  'KEVER',
  'KEZEL',
  'KEZES',
  'KEZEZ',
  'KHAKI',
  'KIBIC',
  'KIBLI',
  'KICSI',
  'KIDOB',
  'KIDUG',
  'KIEJT',
  'KIFEJ',
  'KIFEN',
  'KIFLI',
  'KIFOG',
  'KIFON',
  'KIFUT',
  'KIHAL',
  'KIHAT',
  'KIHOZ',
  'KIIRT',
  'KIJUT',
  'KIKAP',
  'KIKEL',
  'KIKEN',
  'KILEL',
  'KILES',
  'KILOP',
  'KIMAR',
  'KIMER',
  'KIMOS',
  'KINCS',
  'KININ',
  'KINTI',
  'KIOLD',
  'KIOLT',
  'KIONT',
  'KIRAK',
  'KITIN',
  'KITLI',
  'KITOL',
  'KITUD',
  'KIVAN',
  'KIVER',
  'KIVET',
  'KIVON',
  'KLAKK',
  'KLIKK',
  'KLOTT',
  'KOBAK',
  'KOBOZ',
  'KOBRA',
  'KOCKA',
  'KOCOG',
  'KOCSI',
  'KOHOL',
  'KOKAS',
  'KOKSZ',
  'KOLNA',
  'KOLOP',
  'KOMOR',
  'KONDA',
  'KONOK',
  'KONTY',
  'KONYA',
  'KOPEK',
  'KOPIK',
  'KOPJA',
  'KOPOG',
  'KOPRA',
  'KORAI',
  'KORCS',
  'KOROG',
  'KOROM',
  'KOROS',
  'KORPA',
  'KORTY',
  'KOSTA',
  'KOSZT',
  'KOTLA',
  'KOTOL',
  'KOTON',
  'KOTOR',
  'KOTTA',
  'KOTTY',
  'KOZMA',
  'KRACH',
  'KREOL',
  'KREPP',
  'KROKI',
  'KUCIK',
  'KUGLI',
  'KUJON',
  'KUKAC',
  'KUKTA',
  'KULCS',
  'KUPAC',
  'KUPAK',
  'KUPEC',
  'KUPON',
  'KURTA',
  'KURUC',
  'KURVA',
  'KUSTI',
  'KUSZA',
  'KUTAT',
  'KUTYA',
  'KUVIK',
  'KVARC',
  'KVART',
  'KVASZ',
  'KVINT',
  'KVITT',
  'LABDA',
  'LADIK',
  'LAGZI',
  'LAJBI',
  'LAJHA',
  'LAKAT',
  'LAKIK',
  'LAKLI',
  'LAKOL',
  'LAKOS',
  'LAKTA',
  'LANGY',
  'LANKA',
  'LAPKA',
  'LAPOS',
  'LAPOZ',
  'LAPTA',
  'LAPUL',
  'LASKA',
  'LATIN',
  'LATOL',
  'LATOR',
  'LAZAC',
  'LAZUL',
  'LEBEG',
  'LEBKE',
  'LEBUJ',
  'LECKE',
  'LEDOB',
  'LEEJT',
  'LEFED',
  'LEFOG',
  'LEFUT',
  'LEGEL',
  'LEHAT',
  'LEHEL',
  'LEHET',
  'LEHOZ',
  'LEINT',
  'LEJUT',
  'LEKAP',
  'LEKEN',
  'LELES',
  'LELET',
  'LELKI',
  'LELOP',
  'LEMAR',
  'LEMER',
  'LEMEZ',
  'LEMOS',
  'LENGE',
  'LENNI',
  'LENTI',
  'LEOLD',
  'LEOLT',
  'LEPEL',
  'LEPKE',
  'LEPRA',
  'LERAK',
  'LETOL',
  'LETUD',
  'LEVAN',
  'LEVER',
  'LEVES',
  'LEVET',
  'LEVON',
  'LIBEG',
  'LIBUC',
  'LIGET',
  'LIHEG',
  'LILIK',
  'LISTA',
  'LISZT',
  'LITER',
  'LITYI',
  'LOBOG',
  'LOBOS',
  'LOHAD',
  'LOHOL',
  'LOKNI',
  'LOMHA',
  'LOPVA',
  'LOVAG',
  'LOVAL',
  'LOVAS',
  'LUDAS',
  'LUESZ',
  'LUGAS',
  'LUMEN',
  'LUSTA',
  'LUTRI',
  'LUXUS',
  'MACCS',
  'MAFLA',
  'MAGAS',
  'MAGOL',
  'MAGOS',
  'MAGOZ',
  'MAJOM',
  'MAJOR',
  'MAKOG',
  'MAKRA',
  'MALAC',
  'MALOM',
  'MANCS',
  'MANNA',
  'MAPPA',
  'MARAD',
  'MARAT',
  'MARHA',
  'MARJA',
  'MAROK',
  'MARXI',
  'MASNI',
  'MASZK',
  'MATAT',
  'MAZNA',
  'MECCS',
  'MEDER',
  'MEDVE',
  'MEGAD',
  'MEGIN',
  'MEGUN',
  'MEGYE',
  'MEGGY',
  'MEKEG',
  'MELEG',
  'MELLY',
  'MENET',
  'MENTA',
  'MENTE',
  'MENTI',
  'MENZA',
  'MENNY',
  'MERED',
  'MEREV',
  'MERRE',
  'MERSZ',
  'METSZ',
  'MEZEI',
  'MIATT',
  'MIENK',
  'MIKOR',
  'MINAP',
  'MINEK',
  'MINTA',
  'MIRHA',
  'MIVEL',
  'MODOR',
  'MOGUL',
  'MOHAR',
  'MOHOS',
  'MOKKA',
  'MONDA',
  'MORAJ',
  'MOROG',
  'MORVA',
  'MOTEL',
  'MOTOR',
  'MOTOZ',
  'MOZIS',
  'MOZOG',
  'MUCUS',
  'MUHAR',
  'MULAT',
  'MULYA',
  'MUMUS',
  'MUNKA',
  'MURCI',
  'MURIS',
  'MUROK',
  'MURVA',
  'MUTAT',
  'MUTYI',
  'NAFTA',
  'NAIVA',
  'NAPOS',
  'NEMDE',
  'NEMES',
  'NEMEZ',
  'NESZE',
  'NETEK',
  'NEVEL',
  'NEVES',
  'NEVET',
  'NEVEZ',
  'NEXUS',
  'NIMFA',
  'NINCS',
  'NORMA',
  'NOSZA',
  'NUDLI',
  'NULLA',
  'NYEKK',
  'NYELV',
  'NYERS',
  'NYERT',
  'NYEST',
  'NYLON',
  'NYOLC',
  'OBSIT',
  'ODAAD',
  'ODAKI',
  'ODVAS',
  'OKKER',
  'OKTAT',
  'OLASZ',
  'OLDAL',
  'OLDAT',
  'OLDOZ',
  'OLVAD',
  'OLVAS',
  'OLYAN',
  'OLYAS',
  'OLYIK',
  'OMEGA',
  'OMLIK',
  'ONKLI',
  'ONNAN',
  'ONOKA',
  'OPERA',
  'OPUSZ',
  'ORDAS',
  'ORGIA',
  'OROSZ',
  'ORROL',
  'ORVOS',
  'ORVUL',
  'OSTOR',
  'OSTYA',
  'OSZOL',
  'OTTAN',
  'PACAL',
  'PACKA',
  'PACNI',
  'PACSI',
  'PADKA',
  'PADOL',
  'PAIZS',
  'PAJOR',
  'PAJTA',
  'PAJTI',
  'PAJZS',
  'PAKLI',
  'PAKOL',
  'PALOL',
  'PAMAT',
  'PAMPA',
  'PAMUT',
  'PANCS',
  'PAPOL',
  'PAPOS',
  'PARAJ',
  'PARTE',
  'PARTI',
  'PASAS',
  'PASSZ',
  'PATAK',
  'PAUZA',
  'PAZAL',
  'PAZAR',
  'PECEK',
  'PEDER',
  'PEDIG',
  'PENEG',
  'PENGE',
  'PENNA',
  'PEREC',
  'PEREG',
  'PEREL',
  'PEREM',
  'PERES',
  'PERGE',
  'PERJE',
  'PERMI',
  'PERON',
  'PERTU',
  'PESEL',
  'PESTI',
  'PETIT',
  'PETTY',
  'PIACI',
  'PIANO',
  'PIARC',
  'PICSA',
  'PIHEG',
  'PIHEN',
  'PILIS',
  'PILLA',
  'PILLE',
  'PINCE',
  'PINCS',
  'PINKA',
  'PINTY',
  'PIRIT',
  'PIROS',
  'PIRUL',
  'PISIL',
  'PISIS',
  'PISLA',
  'PISZE',
  'PISZI',
  'PISSZ',
  'PISZT',
  'PITAR',
  'PITIZ',
  'PITLE',
  'PITLI',
  'PLACC',
  'PLUSZ',
  'POCAK',
  'POCOK',
  'POFON',
  'POFOZ',
  'POHOS',
  'POKLA',
  'POKOL',
  'POLIP',
  'POLKA',
  'POLOS',
  'POMPA',
  'PONTY',
  'POPSI',
  'POROL',
  'POROS',
  'POROZ',
  'PORTA',
  'POSTA',
  'POSZT',
  'POTOM',
  'POTYA',
  'PREPA',
  'PRIOR',
  'PROCC',
  'PROFI',
  'PROLI',
  'PUCER',
  'PUCOL',
  'PUCCS',
  'PUDLI',
  'PUDVA',
  'PUFOG',
  'PUHUL',
  'PULYA',
  'PUMPA',
  'PUNCI',
  'PUNCS',
  'PUSKA',
  'PUSZI',
  'PUTRI',
  'QUART',
  'QUINT',
  'QUOTA',
  'RABBI',
  'RABOL',
  'RACKA',
  'RADAR',
  'RAFIA',
  'RAGAD',
  'RAGOS',
  'RAGOZ',
  'RAGYA',
  'RAJTA',
  'RAKAT',
  'RANDA',
  'RAPLI',
  'RASSZ',
  'REBEG',
  'RECCS',
  'REDUT',
  'REKED',
  'REKEG',
  'REMEG',
  'REMEK',
  'REMIZ',
  'RENDI',
  'REPCE',
  'REPED',
  'REPES',
  'REPTE',
  'RESTI',
  'RETEK',
  'REUMA',
  'REVES',
  'REZEG',
  'REZEL',
  'REZES',
  'REZEZ',
  'REZSI',
  'RIADT',
  'RIDEG',
  'RIGLI',
  'RIPSZ',
  'RISKA',
  'RITKA',
  'RIVAL',
  'ROBAJ',
  'ROBOG',
  'ROBOT',
  'ROHAD',
  'ROHAM',
  'ROHAN',
  'ROKKA',
  'ROKON',
  'ROMOL',
  'ROMOS',
  'RONCS',
  'RONDA',
  'RONGY',
  'ROPOG',
  'ROSTA',
  'ROSSZ',
  'ROVAR',
  'ROVAT',
  'RUBEL',
  'RUBIN',
  'RUDAL',
  'RUDAS',
  'RUDAZ',
  'RUMLI',
  'RUMOS',
  'RUTIN',
  'SACCO',
  'SAJKA',
  'SAJNA',
  'SAJOG',
  'SALAK',
  'SANDA',
  'SAPKA',
  'SARKI',
  'SAROK',
  'SASFA',
  'SAVAS',
  'SEBAJ',
  'SEBES',
  'SEBEZ',
  'SEDRE',
  'SEHOL',
  'SEHUN',
  'SELMA',
  'SELYP',
  'SEMMI',
  'SENKI',
  'SEPER',
  'SEREG',
  'SERIF',
  'SERKE',
  'SERTE',
  'SIFLI',
  'SIFON',
  'SIKER',
  'SIKET',
  'SIMLI',
  'SIMUL',
  'SINCS',
  'SINUS',
  'SIPEG',
  'SIPKA',
  'SIRAT',
  'SISAK',
  'SISKA',
  'SKALP',
  'SKART',
  'SKICC',
  'SKURC',
  'SLEPP',
  'SLICC',
  'SLUKK',
  'SMARN',
  'SMOKK',
  'SNEFF',
  'SODOR',
  'SOHOL',
  'SOHSE',
  'SOMFA',
  'SOMMA',
  'SONKA',
  'SORBA',
  'SORFA',
  'SOROL',
  'SOROS',
  'SOROZ',
  'SORRA',
  'SOSEM',
  'SPAHI',
  'SPEJZ',
  'SPICC',
  'SPION',
  'SPORT',
  'STAND',
  'START',
  'STERC',
  'SUFNI',
  'SUHAN',
  'SUHOG',
  'SUMMA',
  'SUNKA',
  'SUNYI',
  'SUSKA',
  'SUSOG',
  'SUTTY',
  'SUVAD',
  'SVUNG',
  'SYREN',
  'SZAFT',
  'SZAKA',
  'SZAKI',
  'SZALU',
  'SZAPU',
  'SZARU',
  'SZARV',
  'SZEBB',
  'SZEGY',
  'SZENT',
  'SZERB',
  'SZERV',
  'SZESZ',
  'SZETT',
  'SZEXT',
  'SZIKE',
  'SZIKI',
  'SZILA',
  'SZINT',
  'SZIRT',
  'SZITA',
  'SZNOB',
  'SZOBA',
  'SZOCI',
  'SZOMJ',
  'SZUKA',
  'SZUSZ',
  'SZVIT',
  'TAGAD',
  'TAGOL',
  'TAGOS',
  'TAGOZ',
  'TAJGA',
  'TAKAR',
  'TAKSA',
  'TALAJ',
  'TALAP',
  'TALMI',
  'TALON',
  'TANTI',
  'TANUL',
  'TANYA',
  'TAPAD',
  'TAPOD',
  'TAPOG',
  'TAPOS',
  'TARAJ',
  'TARJA',
  'TARKA',
  'TAROL',
  'TASAK',
  'TATUS',
  'TAVAS',
  'TEGEZ',
  'TEHER',
  'TEINS',
  'TEJEL',
  'TEJES',
  'TEKER',
  'TELEK',
  'TELEL',
  'TELEP',
  'TELIK',
  'TELJE',
  'TELVE',
  'TEMET',
  'TENOR',
  'TENTA',
  'TENTE',
  'TEPER',
  'TEPSI',
  'TEREH',
  'TEREL',
  'TEREM',
  'TEREP',
  'TESTI',
  'TETEM',
  'TETET',
  'TIARA',
  'TILDE',
  'TILOL',
  'TILOS',
  'TINCS',
  'TINTA',
  'TIPEG',
  'TIPLI',
  'TIPOR',
  'TISZT',
  'TITOK',
  'TIZED',
  'TOBOZ',
  'TOJIK',
  'TOKOS',
  'TOLAT',
  'TOLLU',
  'TOLUL',
  'TOMPA',
  'TONNA',
  'TOPOG',
  'TORMA',
  'TORNA',
  'TOROK',
  'TOROL',
  'TOROZ',
  'TORTA',
  'TORZS',
  'TOTEM',
  'TOTYA',
  'TOXIN',
  'TRAPP',
  'TREFF',
  'TROLI',
  'TROMF',
  'TRUCC',
  'TRUPP',
  'TUBUS',
  'TUCAT',
  'TUDAT',
  'TUDOR',
  'TUDTA',
  'TULOK',
  'TUNYA',
  'TURFA',
  'TURHA',
  'TURUL',
  'TUSOL',
  'TUTAJ',
  'TUTUL',
  'TUTYI',
  'UCCSE',
  'UDVAR',
  'UGRAT',
  'UGRIK',
  'UGYAN',
  'UGYSE',
  'UJGUR',
  'UJJAS',
  'UJUJU',
  'ULTRA',
  'UNCIA',
  'UNDOK',
  'UNDOR',
  'UNOKA',
  'UNOTT',
  'UNTAT',
  'UNTIG',
  'URACS',
  'URALG',
  'USGYI',
  'USTOR',
  'UTCAI',
  'VACAK',
  'VACOG',
  'VACOK',
  'VADAS',
  'VADON',
  'VADUL',
  'VAGON',
  'VAJAS',
  'VAJAZ',
  'VAJDA',
  'VAJHA',
  'VAJMI',
  'VAJON',
  'VAKAR',
  'VAKFA',
  'VAKOG',
  'VAKOL',
  'VAKSI',
  'VAKUL',
  'VALAG',
  'VARAS',
  'VARGA',
  'VARSA',
  'VASAL',
  'VASAS',
  'VASAZ',
  'VASFA',
  'VATAT',
  'VATTA',
  'VEDEL',
  'VEDER',
  'VEGYI',
  'VEKNI',
  'VELIN',
  'VELLA',
  'VEMHE',
  'VEREM',
  'VERES',
  'VERET',
  'VESZT',
  'VETET',
  'VEZET',
  'VIASZ',
  'VIDOR',
  'VIDRA',
  'VIDUL',
  'VIGAD',
  'VIHAR',
  'VIHOG',
  'VILLA',
  'VIOLA',
  'VIRAD',
  'VIRUL',
  'VISEL',
  'VITAT',
  'VITEL',
  'VITET',
  'VITLA',
  'VIZEL',
  'VIZES',
  'VIZEZ',
  'VIZIT',
  'VODKA',
  'VOGUL',
  'VOLNA',
  'VOLTA',
  'VONAL',
  'VONAT',
  'VONUL',
  'VUKLI',
  'VULGO',
  'VURST',
  'YACHT',
  'ZABLA',
  'ZABOL',
  'ZABOS',
  'ZAJOG',
  'ZAJOS',
  'ZAMAT',
  'ZAVAR',
  'ZEBRA',
  'ZENEG',
  'ZENEI',
  'ZENIT',
  'ZERGE',
  'ZILIZ',
  'ZIZEG',
  'ZOKNI',
  'ZOKOG',
  'ZOKON',
  'ZUBOG',
  'ZUHAN',
  'ZUHOG',
  'ZSALU',
  'ZSARU',
  'ZSENI',
  'ZSOLD',
  'ZSONG',
  'ZSUFA',
  'ZSUGA',
  'ZSUPP',
]

let morse = {
  laststart: 0,
  down: 0,
  ctx: null,
  gain: null,
  word: '',
  pos: 0,
  code: {
    'A': '.-',
    'B': '-...',
    'C': '-.-.',
    'D': '-..',
    'E': '.',
    'F': '..-.',
    'G': '--.',
    'H': '....',
    'I': '..',
    'J': '.---',
    'K': '-.-',
    'L': '.-..',
    'M': '--',
    'N': '-.',
    'O': '---',
    'P': '.--.',
    'Q': '--.-',
    'R': '.-.',
    'S': '...',
    'T': '-',
    'U': '..-',
    'V': '...-',
    'W': '.--',
    'X': '-..-',
    'Y': '-.--',
    'Z': '--..',
  },
  timeoutID: null,
  // currently entered signals.
  entered: '',

  init: _ => {
    morse.pos = 0
    morse.entered = ''
    let wl = words5
    morse.word = wl[Math.floor(Math.random() * wl.length)]
  },

  toughen: _ => {},

  onkeydown: evt => {
    if (morse.ctx == null) {
      // set up audio.
      // must happen as part of an event handler.
      morse.ctx = new AudioContext()
      morse.gain = morse.ctx.createGain()
      morse.gain.gain.setValueAtTime(0.001, morse.ctx.currentTime)
      morse.gain.connect(morse.ctx.destination)
      morse.osc = morse.ctx.createOscillator()
      morse.osc.connect(morse.gain)
      morse.osc.start()
    }
    morse.ctx.resume()

    // replay the current letter's morse code.
    if (evt.key == 'Enter') {
      let ch = morse.word[morse.pos]
      let t = morse.ctx.currentTime + 0.1
      for (let c of morse.code[ch]) {
        morse.gain.gain.setValueAtTime(0.001, t)
        morse.gain.gain.linearRampToValueAtTime(1, t + 0.01)
        t += 0.11
        if (c == '-') t += 0.2
        morse.gain.gain.setValueAtTime(1, t)
        morse.gain.gain.linearRampToValueAtTime(0.001, t + 0.01)
        t += 0.11
      }
      return
    }

    // reset state if needed.
    let tm = Date.now()
    if (tm - morse.laststart > 1000) {
      morse.oksig = 0
    }
    morse.laststart = tm

    // start beeping.
    if (evt.key != ' ' || morse.down != 0) return
    clearTimeout(morse.timeoutID)
    morse.down = tm
    let t = morse.ctx.currentTime
    morse.gain.gain.setValueAtTime(0.001, t + 0.02)
    morse.gain.gain.linearRampToValueAtTime(1, t + 0.04)
  },

  onkeyup: evt => {
    if (evt.key != ' ') return
    clearTimeout(morse.timeoutID)

    // detect the signal length and stop beeping.
    let len = Date.now() - morse.down
    morse.down = 0
    let t = morse.ctx.currentTime
    morse.gain.gain.setValueAtTime(1, t + 0.02)
    morse.gain.gain.linearRampToValueAtTime(0.001, t + 0.04)

    // process the signal.
    let sig = '.'
    if (len > 200) sig = '-'
    if (len > 600) sig = 'x'
    morse.entered += sig
    let code = morse.code[morse.word[morse.pos]]
    if (morse.entered == morse.code[morse.word[morse.pos]] || !morse.code[morse.word[morse.pos]].startsWith(morse.entered)) {
      morse.check()
    } else {
      morse.timeoutID = setTimeout(morse.check, 1000)
    }
  },

  check: _ => {
    if (morse.entered == morse.code[morse.word[morse.pos]]) morse.pos++
    if (morse.pos == morse.word.length) {
      morse.ctx.suspend()
      winstate()
    }
    morse.entered = ''
    morse.render()
  },

  render: _ => {
    let h = ''
    for (let i in morse.word) {
      let ch = morse.word[i]
      h += i == morse.pos ? '→' : ' '
      h += `${ch}\n`
    }
    hchallenge.innerText = h
  }
}

function keydown(evt) {
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

let challenge = nback
main()
