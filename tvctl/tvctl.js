function reward() {
  hchallenge.hidden = true;
  hcorrectmsg.hidden = true;
  window.onkeydown = null;
  fetch('/reward', {method: 'POST'});
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

let challenge = glyphs;
if (challenge.init) challenge.init();
window.onkeydown = evt => {
  if (evt.altKey || evt.ctrlKey) return;
  if (challenge.onkeydown) {
    challenge.onkeydown(evt);
    challenge.render();
  }
};
challenge.render();
