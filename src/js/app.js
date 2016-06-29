var myAPIKey = 'f7d66a3ed7d5e63a8358207b59518652';
var undergroundKey = '9854ee2d7ac265c0';


if(localStorage.getItem('useWug') === null){
    localStorage.setItem('useWug', '0');
}else{
    if(localStorage.getItem('useWug') === "1" && localStorage.getItem('myAPIKey') === null){    
        // Store some data
        localStorage.setItem('myAPIKey', undergroundKey);
    }    
}

if( localStorage.getItem('myAPIKey') === null){    
    // Store some data
    localStorage.setItem('myAPIKey', myAPIKey);
}

if(localStorage.getItem('btOn') === null){
    localStorage.setItem('btOn', '1');
}


if(localStorage.getItem('qTime') === null){
    localStorage.setItem('qTime', '0');
}

if(localStorage.getItem('sTime') === null){
    localStorage.setItem('sTime', '0');
}

if(localStorage.getItem('eTime') === null){
    localStorage.setItem('eTime', '0');
}


var ajax = function (url, type, success, error){
  var xhr = new XMLHttpRequest();
  
  xhr.onload = function () {
    var json = JSON.parse(this.responseText);
    success(json);
  };
  
  xhr.onerror = function() {
    error(this.responseText);
  };
  
  xhr.open(type, url);
  xhr.send();
};

if (!String.prototype.startsWith) {
    String.prototype.startsWith = function(searchString, position){
      position = position || 0;
      return this.substr(position, searchString.length) === searchString;
  };
}

function sendMessageBack(tempF, conditions, isDay){ //, btOn, qTime, sTime, eTime){
    var btOn = localStorage.getItem('btOn');    
    var qTime = localStorage.getItem('qTime');
    var sTime = localStorage.getItem('sTime');
    var eTime = localStorage.getItem('eTime');
    
    // Assemble dictionary using our keys
    var dictionary = {
        'KEY_TEMPERATURE': tempF,
        'KEY_CONDITIONS': conditions,
        'KEY_DAY' : isDay,
        'KEY_BT': btOn,
        'KEY_Q_TIME': qTime,
        'KEY_S_TIME': sTime.replace(" ", "").replace(" ",""),
        'KEY_E_TIME': eTime.replace(" ", "").replace(" ","")
    };
    
    console.log("Sending Messag with: ");
    console.log('KEY_TEMPERATURE: ' + tempF);
    console.log('KEY_CONDITIONS: ' + conditions);
    console.log('KEY_DAY: ' + isDay);
    console.log('KEY_BT: ' + btOn);
    console.log('KEY_Q_TIME: ' + qTime);
    console.log('KEY_S_TIME: ' + sTime);
    console.log('KEY_E_TIME: ' + eTime);
   
    // Send to Pebble
    Pebble.sendAppMessage(dictionary,
                          function(e) {
                              console.log('Weather info sent to Pebble successfully!');
                          },
                          function(e) {
                              console.log('Error sending weather info to Pebble!');
                          }
                         );
}

/*
    0 = clear
    1 = cloud
    2 = partly
    3 = foggy
    4 = rain
    5 = thunder
*/
function getConditionCodeOpenWeather(weatherId){
    var weatherCode = 0;
    
    if( weatherId < 300 ) {
        weatherCode = 5;
    } else if( weatherId < 600 ) {
        weatherCode = 4;
    } else if( weatherId < 700 ) { /* SNOW */
        weatherCode = 1;
    } else if( weatherId < 800 ) {
        weatherCode = 3;
   } else if( weatherId == 800) {
        weatherCode = 0;
    } else if( weatherId < 804 ) {
        weatherCode = 2;
    } else if( weatherId < 810 ) {
        weatherCode = 1;
    } else if( weatherId < 910 ) { /*EXTREME*/
        weatherCode = 1;
    } else {
        weatherCode = 0;
    }
    
    return weatherCode;
}

function getConditionCodeUnderGround(iconName){
    var weatherCode = 0;
    
    switch(iconName){
        case 'clear':
        case 'sunny':
            weatherCode = 0;
            break;
            
        case 'cloudy':
            weatherCode = 1;
            break;
            
        case 'mostlycloudy':
        case 'mostlysunny':
        case 'partlycloudy':
        case 'partlysunny':
            weatherCode = 2;
            break;
            
        case 'fog':
        case 'hazy':
            weatherCode = 3;
            break;
            
        case 'chanceflurries':
        case 'chancesnow':
        case 'snow':
        case 'flurries':
            weatherCode = 4;
            break;
            
        case 'chancerain':
        case 'chancesleet':
        case 'chancetstorms':
        case 'sleet':
        case 'rain':
            weatherCode = 4;
            break;
            
        case 'tstorms':
        case 'unknown':
            weatherCode = 5;
            break;
            
        default:
            weatherCode = 0;
            break;
    }
        
    return weatherCode;
}

function getWeatherUnderGroundCondition(pos){
    var condition = 0;
    
    // We will request the weather here
    var apiKey = localStorage.getItem('myAPIKey');
    
    var url = 'http://api.wunderground.com/api/'+ apiKey +'/conditions/conditions/alert/q/' + pos.coords.latitude + ',' + pos.coords.longitude + '.json';

    ajax(
        url,
        'GET',
        function(data) {
                var current = data.current_observation;
                var weatherIcon = current.icon;
                var tempF = current.temp_f;

                // Conditions
                var conditions = getConditionCodeUnderGround(weatherIcon);
                
                var isDay = !(new RegExp('.*/nt_.*.gif')).test(current.icon_url) ? 0 : 1;

                sendMessageBack(tempF, conditions, isDay);
            },
            function(error) {
                console.log('Download failed: ' + error);
            }
        );   
    
    return condition;
}

function IsDay(sunriseMs, sunsetMs){
    var sunrise = new Date(sunriseMs * 1000);
    var sunset = new Date(sunsetMs * 1000);
    var now = new Date();
    
    if(sunrise > now || now > sunset){
        return 1;
    }
    
    return 0;
}

function getOpenWeatherCondition(pos) {
    // We will request the weather here
    var apiKey = localStorage.getItem('myAPIKey');
    if(apiKey !== null){
        // Construct URL
        var url = 'http://api.openweathermap.org/data/2.5/weather?lat=' +
            pos.coords.latitude + '&lon=' + pos.coords.longitude + '&appid=' + apiKey;

        ajax(
            url,
            'GET',
            function(data) {
                // Temperature in Kelvin requires adjustment
                var temperature = Math.round(data.main.temp - 273.15);
                temperature = Math.round((temperature * 9 / 5 + 32));

                // Conditions
                var conditions = getConditionCodeOpenWeather(data.weather[0].id);

                var isDay = IsDay(data.sys.sunrise, data.sys.sunset);
                
                sendMessageBack(temperature, conditions, isDay);
            },
            function(error) {
                console.log('Download failed: ' + error);
            }
        );   
    }else{            
        sendMessageBack(0, 0, 0);
    }
}

function locationSuccess(pos){
    var useWug = localStorage.getItem('useWug') === '1';
    
    if(useWug){
        getWeatherUnderGroundCondition(pos);
    }else{
        getOpenWeatherCondition(pos);  
    }
}

function locationError(err) {
  console.log('Error requesting location!');
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 60000}
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log('PebbleKit JS ready!');
    getWeather();
  }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
                        function(e) {
                            // Get the initial weather
                            console.log('AppMessage received! Getting weahter');
                            getWeather();
                        }                     
);

Pebble.addEventListener('showConfiguration', function() {
    var url = 'https://weathersteptime.herokuapp.com/';
    
    var apiKey = localStorage.getItem('myAPIKey');
    
    var btOn = localStorage.getItem('btOn');
    var useWug = localStorage.getItem('useWug');
    
    var qTime = localStorage.getItem('qTime');
    var sTime = localStorage.getItem('sTime');
    var eTime = localStorage.getItem('eTime');
        
    if(btOn !== null){
        url += btOn;
    }else{
        url += "0";
    }
    
    if(useWug !== null){
        url += "/"+useWug;
    }else{
        url += "/"+"0";
    }
    
    if(qTime !== null){
        url += "/"+qTime;
    }else{
        url += "/"+"0";
    }
    
    if(sTime !== null){
        url += "/"+sTime;
    }else{
        url += "/"+"0";
    }
    
    if(eTime !== null){
        url += "/"+eTime;
    }else{
        url += "/"+"0";
    }
    
    if(apiKey !== null && myAPIKey !== apiKey){
        url += "/"+apiKey;
    }else{
        url += "/no";
    }
    
    Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
    // Decode the user's preferences
    var configData = JSON.parse(decodeURIComponent(e.response));
    
    var useWug = configData.useWug;
    
    if(useWug !== ""){
        localStorage.setItem('useWug', useWug);
    }
    
    var apiKey = configData.api_key;
    
    if(apiKey !== "" && apiKey !== undefined){
        localStorage.setItem('myAPIKey', apiKey);
    }else{        
        localStorage.setItem('myAPIKey', myAPIKey);
    }
    
    var btOn = configData.bt_note;
    if(btOn !== ""){
        localStorage.setItem('btOn', btOn);
    }
    
    var qTime = configData.qTime;
    var sTime = configData.sTime;
    var eTime = configData.eTime;
    
    if(qTime !== ""){
        localStorage.setItem('qTime', qTime);
    }
    
    if(sTime !== "" && sTime !== undefined){
        localStorage.setItem('sTime', sTime);
    }
    
    if(eTime !== "" && eTime !== undefined){
        localStorage.setItem('eTime', eTime);
    }
    
    getWeather();
});
