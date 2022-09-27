var escape_map = {
 '&amp;': '&',
 '&lt;': '<',
 '&gt;': '>',
 '&quot;': '"',
 '&#x27;': "'",
 '&#x60;': "`", 
 '&#8217;': "'", 
};

function unescaper() {
  let source = '(?:' + Object.keys(escape_map).join('|') + ')';
  let testRegexp = RegExp(source);
  let replaceRegexp = RegExp(source, 'g');
  return function(string) {
     string = string == null ? '' : '' + string;
     return testRegexp.test(string) ? string.replace(replaceRegexp, function(match) { return escape_map[match]; }) : string;
  };
}
var unescape = unescaper()

var labels = {
	"day": "DAY",
	"hour": "TIME",
	"title": "TITLE",
	"speaker": "SPEAKER",
	"location": "LOCATION",
	"duration": "DURATION"
};


function setInput(input, value){
  input.value = value;
}

function resetInput(input){
  setInput(input, '');
}

function getCookie(name) {
  const value = `; ${document.cookie}`;
  const parts = value.split(`; ${name}=`);
  if (parts.length === 2) return parts.pop().split(';').shift();
}

function setCookie(name, value, expDays) {
  let date = new Date();
  date.setTime(date.getTime() + (expDays * 24 * 60 * 60 * 1000));
  const expires = "expires=" + date.toUTCString();
  document.cookie = name + "=" + value + "; " + expires + "; path=/";
  return getCookie(name);
}

function removeCookie(name) {
  setCookie(name, '', -1);
}

function generate_head(table, data) {
    let thead = table.createTHead();
    let row = thead.insertRow();
    for (let key of data) {
      let th = document.createElement("th");
      let text = document.createTextNode(key);
      th.appendChild(text);
      row.appendChild(th);
    }
  }

  function generate_table(table, data, row_keys) {
    let tbody = table.createTBody();
    for (let element of data) {
      let row = tbody.insertRow();
      row_keys.forEach(key =>{
        let cell = row.insertCell();
        let text = document.createTextNode(element[key]);
        cell.appendChild(text);
	cell.setAttribute("data-column", labels[key]);
        cell.innerText = unescape(element[key]);
      });
    }
  }

function append_table(data, table_id, list_order) {
    let headings_title = list_order.map(x => Object.values(x)[0]);
    let row_keys = list_order.map(x => Object.keys(x)[0]);

    let table = document.getElementById(table_id);
    //let headings = Object.keys(data[0]);
    generate_head(table, headings_title);
    generate_table(table, data, row_keys);
    return table;
}

function getRssiInt(rssi_string){
  return parseInt(rssi_string.split('dBm')[0]);
}

function getTopRssi(rssiObj, top=10){
  return rssiObj.sort((a,b) => getRssiInt(b.rssi) - getRssiInt(a.rssi)).slice(0, top);
}

function getRndInteger(min, max) {
  max = max + 1;
  return Math.floor(Math.random() * (max - min) ) + min;
}

function getRndSign() {
  let r = getRndInteger(0, 1)
  if (r == 0){
    return -1;
  }
  else{
    return 1;
  }
}

function rssiToCoordinates(rssi){
  let mid_range = -50;
  let near_range = -30;

  let top = 0;
  let left = 0;
  if (rssi < mid_range){
    
    let upper_bound = 30;
    let lower_bound = 20;
    top = 50 + getRndInteger(lower_bound, upper_bound) * getRndSign();
    left = 50 + getRndInteger(lower_bound, upper_bound) * getRndSign();
  } else if ((mid_range <= rssi) && (rssi < near_range)){
    
    let upper_bound = 20;
    let lower_bound = 10;
    top = 50 + getRndInteger(lower_bound, upper_bound) * getRndSign();
    left = 50 + getRndInteger(lower_bound, upper_bound) * getRndSign();
  } else if (near_range <= rssi){
    
    let upper_bound = 10;
    let lower_bound = 0;
    top = 50 + getRndInteger(lower_bound, upper_bound) * getRndSign();
    left = 50 + getRndInteger(lower_bound, upper_bound) * getRndSign();
  }
  return [top, left];
}

function setRadarBalls(badge_list){
  let radar = document.getElementsByClassName('radar-content')[0];
  
  badge_list.forEach(badge => {
    let ball = document.createElement("div");
    ball.classList.add("radar-ball");
    ball.innerHTML = badge.id;
    let rssi = getRssiInt(badge.rssi);
    let coords = rssiToCoordinates(rssi);
    
    ball.style.cssText += 'top:' + coords[0] + '%;left:' + coords[1] + '%;'
    
    radar.appendChild(ball);
  });

}

function radar_reset() {
  let radar = document.getElementsByClassName('radar-content')[0];
  while (radar.firstChild) radar.removeChild(radar.firstChild);
  let pos = document.createElement("div");
  pos.classList.add("radar-pos");
  radar.appendChild(pos);
}

function radar_table_reset() {
  let els = document.querySelector("#table-range").innerHTML = "";
  document.querySelector("#table-range-container .paginator").remove();
}
