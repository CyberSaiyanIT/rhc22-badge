// Client API goes here
async function query(endpoint, body) {
  let BACKEND = '/backend/';
  return fetch(BACKEND + endpoint, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(body)
  })
  .then(response => {
    if(!response.ok){
      throw new Error('Something went wrong');
    }
    return response.json()
  })
  .catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
}

async function query_schedule() {
  return query('schedule').then(data => {
    return data;
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
}

async function query_event_info() {
  return query('schedule').then(data => {
    if(data.result != 'success'){
      throw new Error('The request returned ' + data.result);
    } 
    return data.info;
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
}

async function query_radar() {
  return query('radar').then((data) => {
    return data.radar;
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
}

async function query_authentication(key) {
  return query('check_authentication', {key: key}).then((data) => {
    if(!key){
      throw new Error('Missing key');
    }
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
}

async function query_login(password) {
  return query('login', {password: password}).then((data) => {
    return data.key;
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
}

async function query_logout(key) {
  return query('logout', {key: key}).then((data) => {
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
}

async function query_password(key, password) {
  return query('password', {key: key, password: password}).then((data) => {
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
  //return query('name', {key: key, name: name});
}

async function query_name(key, name) {
  return query('name', {key: key, name: name}).then((data) => {
    return data.name;
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
  //return query('name', {key: key, name: name});
}

async function query_wifi(key, ssid, password) {
  return query('wifi', {key: key, wifi: {ssid: ssid, password: password}}).then((data) => {
    return data.wifi;
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
  //return query('wifi', {key: key, wifi:{ssid: ssid, password: password, status: status}});
}

async function query_sync(key) {
  return query('sync', {key: key}).then((data) => {
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
  //return query('name', {key: key, name: name});
}

async function query_reset(key) {
  return query('reset', {key: key}).then((data) => {
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
  //return query('name', {key: key, name: name});
}

async function query_badge_info() {
  return query('info').then((data) => {
    return data;
  }).catch(error => {
    return Promise.reject(error);//console.error('There has been a problem with your fetch operation:', error);
  });
  //return query('name', {key: key, name: name});
}

//const delay = t => new Promise(resolve => setTimeout(resolve, t));
