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
      if (challenge.onkeydown) window.onkeydown = challenge.onkeydown
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

  onkeydown: evt => {
    if (evt.code == 'ArrowLeft') circles.left = true;
    if (evt.code == 'ArrowRight') circles.right = true;
    if (evt.code == 'ArrowUp') circles.up = true;
    if (evt.code == 'ArrowDown') circles.down = true;
    if (evt.code == 'Enter' && circles.collision != null) {
      hmsg.hidden = true;
      circles.level = 0;
      circles.init();
    }
  },

  onkeyup: evt => {
    if (evt.code == 'ArrowLeft') circles.left = false;
    if (evt.code == 'ArrowRight') circles.right = false;
    if (evt.code == 'ArrowUp') circles.up = false;
    if (evt.code == 'ArrowDown') circles.down = false;
  },

  simulate: _ => {
    const d = 5;
    let won = false;
    if (circles.up || circles.down || circles.left || circles.right) {
      let r = circles.r;
      if (circles.up) circles.cy -= d;
      if (circles.down) circles.cy += d;
      if (circles.left) circles.cx -= d;
      if (circles.right) circles.cx += d;
      if (circles.cx < r) circles.cx = r;
      if (circles.cx > 1800 - r) circles.cx = 1800 - r;
      if (circles.cy < r) circles.cy = r;
      if (circles.cy > 700 - r) circles.cy = 700 - r;
      let x = circles.cx,
        y = circles.cy;
      if (x == 1800 - r && y == 700 - r) {
        won = true;
        if (circles.level <= 1) {
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
    if (dircircles.up || dircircles.down || dircircles.left || dircircles.right) {
      let r = dircircles.r;
      if (dircircles.left) dircircles.cangle -= d / 100.0;
      if (dircircles.right) dircircles.cangle += d / 100.0;
      if (dircircles.up) {
        dircircles.cx += d * Math.cos(dircircles.cangle)
        dircircles.cy += d * Math.sin(dircircles.cangle)
      }
      if (dircircles.down) {
        dircircles.cx -= d * Math.cos(dircircles.cangle)
        dircircles.cy -= d * Math.sin(dircircles.cangle)
      }
      if (dircircles.cx < r) dircircles.cx = r;
      if (dircircles.cx > 1800 - r) dircircles.cx = 1800 - r;
      if (dircircles.cy < r) dircircles.cy = r;
      if (dircircles.cy > 700 - r) dircircles.cy = 700 - r;
      let x = dircircles.cx,
        y = dircircles.cy;
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
        let dx = o[0] - x,
          dy = o[1] - y;
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
  k: 12,
  n: 1,
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
      for (let i = 1; i < nback.n; i++) h += '_ '
    } else if (nback.solved == 0) {
      for (let i = 0; i < nback.n; i++) h += `${nback.nums[i]} `
    } else if (nback.solved < nback.k) {
      for (let i = 0; i < nback.n; i++) h += '_ '
    }
    if (nback.solved + nback.n < nback.k) h += `${nback.nums[nback.solved+nback.n]} `
    for (let i = nback.solved + nback.n; i < nback.k; i++) h += '_ '
    if (nback.solved < nback.k) {
      h += '\n'
      for (let i = 0; i < nback.solved; i++) h += '  '
      h += '^'
    }
    hchallenge.innerHTML = h
  },
}

function main() {
  if (challenge.init) challenge.init();
  window.onkeydown = evt => {
    if (evt.key == 'F5') {
      evt.preventDefault()
      return
    }
    if (evt.altKey || evt.ctrlKey) return;
    if (challenge.onkeydown) {
      challenge.onkeydown(evt);
      challenge.render();
    }
  };
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
