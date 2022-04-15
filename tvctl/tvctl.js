function winstate() {
  hcorrectmsg.hidden = false;
  window.onkeydown = evt => {
    if (evt.altKey || evt.ctrlKey) return;
    if (evt.key == 'Enter') {
      hchallenge.hidden = true;
      hcorrectmsg.hidden = true;
      window.onkeydown = null;
      fetch('/reward', {method: 'POST'});
    }
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

let challenge = starcounting;
if (challenge.init) challenge.init();
window.onkeydown = evt => {
  if (evt.altKey || evt.ctrlKey) return;
  if (challenge.onkeydown) {
    challenge.onkeydown(evt);
    challenge.render();
  }
};
challenge.render();
