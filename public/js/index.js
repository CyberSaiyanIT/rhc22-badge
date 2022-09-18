/* Initialize html elements */
let logo_click_counter = 0;
var rh_logo = document.getElementById("rh-logo");

var login_form = document.querySelector("#login");
var password_input = document.querySelector("#password");
var logout_button = document.querySelector("#btnLogout");
var badge_name_form = document.querySelector("#badge-name-form");
var badge_name_input = document.querySelector("#badge-name");
var wifi_ssid_form = document.querySelector("#wifi-ssid-form");
var wifi_ssid_input = document.querySelector("#wifi-ssid");
var web_password_form = document.querySelector("#web-password-form");
var web_password_input1 = document.querySelector("#web-admin-password1");
var web_password_input2 = document.querySelector("#web-admin-password2");
var wifi_password_form = document.querySelector("#wifi-password-form");
var wifi_password_input1 = document.querySelector("#wifi-password1");
var wifi_password_input2 = document.querySelector("#wifi-password2");
var wifi_toggle_button = document.querySelector("#wifi-toggle");
var sync_button = document.querySelector("#btnForceSync");
var reset_button = document.querySelector("#btnFactoryReset");
var form_authenticated = document.querySelector("#form_authenticated");
var form_not_authenticated = document.querySelector("#form_not_authenticated");
var badge_info_section = document.querySelector("#badge-info");
var event_info_section = document.querySelector("#event-info");

var radar_button_minus = document.querySelector(".radar-buttons button:first-child");
var radar_button_plus = document.querySelector(".radar-buttons button:last-child");

var radar_elements = 10;

var radar_number = document.querySelector("#radar-number");

/* Define app functions */
async function setWifiStatus(statusBool) {
  wifi_toggle_button.checked = statusBool;
}

async function setAuthenticated(statusBool) {
  if(statusBool){
    if (!form_authenticated.style.display == "block") return;
    form_authenticated.style.display = "block";
    form_not_authenticated.style.display = "none";
  } else {
    if (!form_not_authenticated.style.display == "block") return;
    form_not_authenticated.style.display = "block";
    form_authenticated.style.display = "none";
  }
}

async function check_authentication(key) {
  query_authentication(key)
    .then(() => {
      query_name(key, null).then(name => {
        setInput(badge_name_input, name);
      });
      query_wifi(key, null).then(wifi => {
        setInput(wifi_ssid_input, wifi.ssid);
      });
    })
    .then(setAuthenticated(true))
    .catch(() => {
      setAuthenticated(false);
    });
}

async function login(password) {
  query_login(password)
    .then((data) => {
      Toastify({
        text: "Login successful",
        duration: 3000,
        position: "center",
        style: {
          background: "#28a745",
        },
        }).showToast();
        
      return setCookie('key', data, 1);
    })
    .catch(()=>{
      Toastify({
        text: "Login failed",
        duration: 3000,
        position: "center",
        style: {
          background: "#dc3545",
        },
        }).showToast();
    })
    .then(key => check_authentication(key));
}

async function logout(key) {
  query_logout(key)
    .then(() => {
      Toastify({
        text: "Logout successful",
        duration: 3000,
        position: "center",
        style: {
          background: "#28a745",
        },
        }).showToast();
      removeCookie('key')
    })
    .then(() => check_authentication());
}

async function setupSchedule() {
  query_schedule().then(data => {
    decoded = atob(data.info);
    event_info_section.innerHTML = decoded;
    let schedule_data = data.schedule;//.sort((a, b) => a.index - b.index);
    list_order = [{
        "day": "DAY"
      },
      {
        "hour": "TIME"
      },
      {
        "location": "LOCATION"
      },
      {
        "duration": "DURATION"
      },
      {
        "speaker": "SPEAKER"
      },
      {
        "title": "TITLE"
      },
    ];
    let table = append_table(schedule_data, 'table-schedule', list_order);
    let box = paginator({
      table: table,
    });
    box.classList.add("paginator");
    document.getElementById("table-schedule").parentElement.appendChild(box);
  }).catch(error => console.error(error));
}

async function setupEventInfo() {
  query_event_info()
  .then(data => {
    decoded = atob(data);
    event_info_section.innerHTML = decoded;
  })
  .catch(error => console.error(error));
}

async function setupRadar() {
  query_radar().then(data => {
    let radar_data = data.sort((a, b) => getRssiInt(b.rssi) - getRssiInt(a.rssi));
    let list_order = [{
        "id": "ID"
      },
      {
        "name": "NAME"
      },
      {
        "rssi": "RSSI"
      }
    ];

    let table = append_table(radar_data, 'table-range', list_order);
    let box = paginator({
      table: table,
    });
    box.classList.add("paginator");
    document.getElementById('table-range').parentElement.appendChild(box);
    

    setRadarBalls(radar_data.slice(0, radar_elements));
    document.querySelector("#radar-number code").innerHTML = radar_elements;
  }).catch(error => console.error(error));
}

async function setupBadgeInfo() {
  query_badge_info()
  .then(data => {
    for (let [key, value] of Object.entries(data)) {
      let tr = document.createElement("tr");
      let td_k = document.createElement("td");
      let td_v = document.createElement("td");
      td_k.innerHTML = key;
      td_k.style.fontWeight = "bold";
      td_v.innerHTML = value;
      tr.appendChild(td_k);
      tr.appendChild(td_v);
      badge_info_section.appendChild(tr);
    }
  })
  .catch(error => console.error(error))
}

/* Initialize page */
setupSchedule();
setupRadar();
setupBadgeInfo();
check_authentication(getCookie('key'));

setInterval(function() {
  radar_table_reset();
  radar_reset();
  setupRadar();
},5000);  

/* Setup Handlers */
login_form.addEventListener("submit", function (evt) {
  evt.preventDefault();
  login(password_input.value).then(resetInput(password_input));
});

logout_button.addEventListener('click', function (evt) {
  evt.preventDefault();
  logout(getCookie("key"));
});

badge_name_form.addEventListener('submit', function (evt) {
  evt.preventDefault();
  if (badge_name_input.value.length > 0) {
    query_name(getCookie("key"), badge_name_input.value)
    .then(name => {
      setInput(badge_name_input, name);
    })
    .then(() => Toastify({
      text: "Badge name successfully changed. Please reboot the device.",
      duration: 3000,
      position: "center",
      style: {
        background: "#28a745",
      },}).showToast())
    .catch(() => Toastify({
      text: "Error. Cannot change name.",
      duration: 3000,
      position: "center",
      style: {
        background: "#dc3545",
      },
      }).showToast());
  }
});

wifi_ssid_form.addEventListener('submit', function (evt) {
  evt.preventDefault();
  if (wifi_ssid_input.value.length > 0) {
    query_wifi(getCookie("key"), wifi_ssid_input.value, null, null)
    .then(wifi => {setInput(wifi_ssid_input, wifi.ssid)})
    .then(() => Toastify({
      text: "WiFi ssid successfully changed. Please restart AP mode.",
      duration: 3000,
      position: "center",
      style: {
        background: "#28a745",
      },}).showToast())
    .catch(() => Toastify({
      text: "Error. Cannot change ssid.",
      duration: 3000,
      position: "center",
      style: {
        background: "#dc3545",
      },
      }).showToast());
  }
});

web_password_form.addEventListener('submit', function (evt) {
  evt.preventDefault();
  if ((web_password_input1.value.length > 0) && (web_password_input2.value.length > 0)) {
    if (web_password_input1.value === web_password_input2.value) {
      query_password(getCookie("key"), web_password_input1.value)
      .then(data => {
        logout(getCookie("key"));
        resetInput(web_password_input1);
        resetInput(web_password_input2);
      }).then(() => Toastify({
        text: "Login password successfully changed.",
        duration: 3000,
        position: "center",
        style: {
          background: "#28a745",
        },}).showToast())
        .catch(() => Toastify({
        text: "Error. Cannot change login password.",
        duration: 3000,
        position: "center",
        style: {
          background: "#dc3545",
        },
        }).showToast());;
    }
  }
});

wifi_password_form.addEventListener('submit', function (evt) {
  evt.preventDefault();
  if ((wifi_password_input1.value.length > 0) && (wifi_password_input2.value.length > 0)) {
    if (wifi_password_input1.value === wifi_password_input2.value) {
      query_wifi(getCookie("key"), null, wifi_password_input1.value, null)
      .then(() => {
        resetInput(wifi_password_input1);
        resetInput(wifi_password_input2);
      })
      .then(() => Toastify({
        text: "WiFi password successfully changed. Please restart AP mode.",
        duration: 3000,
        position: "center",
        style: {
          background: "#28a745",
        },}).showToast())
      .catch(() => Toastify({
        text: "Error. Cannot change WiFi password.",
        duration: 3000,
        position: "center",
        style: {
          background: "#dc3545",
        },
        }).showToast());;
    }
  }
});


// wifi_toggle_button.addEventListener('change', function (evt) {
//   query_wifi(getCookie("key"), null, null, this.checked).then(wifi => {
//     setWifiStatus(this.checked);
//   });
// });

// sync_button.addEventListener('click', function (evt) {
//   evt.preventDefault();

//   query_sync(getCookie("key")).then(() => {

//   });
// });

reset_button.addEventListener('click', function (evt) {
  evt.preventDefault();

  query_reset(getCookie("key"))
  .then(() => {

  })
  .then(() => Toastify({
    text: "Device is going to reset & reboot...",
    duration: 5000,
    position: "center",
    style: {
      background: "#28a745",
    },}).showToast())
  .catch(() => Toastify({
    text: "Error. Unable to reset.",
    duration: 3000,
    position: "center",
    style: {
      background: "#dc3545",
    },
    }).showToast());;
});

radar_button_minus.addEventListener('click', function (evt) {
  query_radar().then(data => {
      let radar_data = data.sort((a, b) => getRssiInt(b.rssi) - getRssiInt(a.rssi));
      let list_order = [{
        "id": "ID"
      },
      {
        "name": "NAME"
      },
      {
        "rssi": "RSSI"
      }
    ];
    let row_keys = list_order.map(x => Object.keys(x)[0]);
    document.querySelector('#table-range tbody').remove();

    let table = document.getElementById("table-range");
    generate_table(table, data, row_keys);
    let box = paginator({
      table: table,
    });
    box.classList.add("paginator");
    table.parentElement.getElementsByClassName('paginator')[0].remove()
    table.parentElement.appendChild(box);

  
    radar_reset();
    if (radar_elements > 1) setRadarBalls(radar_data.slice(0, --radar_elements));
    document.querySelector("#radar-number code").innerHTML = radar_elements;

  }).catch(error => console.error(error));
});
radar_button_plus.addEventListener('click', function (evt) {
  query_radar().then(data => {

    let radar_data = data.sort((a, b) => getRssiInt(b.rssi) - getRssiInt(a.rssi));
    let list_order = [{
        "id": "ID"
      },
      {
        "name": "NAME"
      },
      {
        "rssi": "RSSI"
      }
    ];
    let row_keys = list_order.map(x => Object.keys(x)[0]);
    document.querySelector('#table-range tbody').remove();

    let table = document.getElementById("table-range");
    generate_table(table, data, row_keys);
    let box = paginator({
      table: table,
    });
    box.classList.add("paginator");
    table.parentElement.getElementsByClassName('paginator')[0].remove()
    table.parentElement.appendChild(box);


    radar_reset();
    setRadarBalls(radar_data.slice(0, ++radar_elements));
    document.querySelector("#radar-number code").innerHTML = radar_elements;

  }).catch(error => console.error(error));
});
radar_number.addEventListener('click', function (evt) {
  query_radar().then(data => {

    let radar_data = data.sort((a, b) => getRssiInt(b.rssi) - getRssiInt(a.rssi));
    let list_order = [{
        "id": "ID"
      },
      {
        "name": "NAME"
      },
      {
        "rssi": "RSSI"
      }
    ];
    let row_keys = list_order.map(x => Object.keys(x)[0]);
    document.querySelector('#table-range tbody').remove();

    let table = document.getElementById("table-range");
    generate_table(table, data, row_keys);
    let box = paginator({
      table: table,
    });
    box.classList.add("paginator");
    table.parentElement.getElementsByClassName('paginator')[0].remove()
    table.parentElement.appendChild(box);


    radar_reset();
    setRadarBalls(radar_data.slice(0, radar_elements));
    document.querySelector("#radar-number code").innerHTML = radar_elements;

  }).catch(error => console.error(error));
});

rh_logo.addEventListener("click", function() {
  if (logo_click_counter > 0) {
    Toastify({
      text: "You are now " + (6-logo_click_counter) + " steps away from being a hacker.",
      duration: 2000,
      position: "center",
      gravity: "bottom",
      style: {
        background: "#fafafa",
        color: "#000000"
      },
      }).showToast();
  }
  logo_click_counter++;
  if(logo_click_counter > 6) document.location.href = "/tetris.html";
});