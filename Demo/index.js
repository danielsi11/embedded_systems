var express = require('express'); //load express module for web application
var socket = require('socket.io');  //load socket.io module for web socket

//Setup web application
var app = express();
var server = app.listen(5000, function(){ //server is running on localhost:5000
    console.log('listening for requests on port 5000');
});

//Initialise table of data
var item = {cur_time:"00:00:00", temp:0, roll:0, pitch:0, max_temp:0, max_roll:0, max_pitch:0};
var oldList = [];

oldList.push(item); //storing the data in an array

//Serve static files in Express
app.use(express.static('public'));

// Setup web socket & pass in server
var io = socket(server);
io.on('connection', function(socket){ //when a web socket connection is established, carry out the following function
    console.log('connected');
    io.sockets.emit('update', oldList); //send the initialised array to the client

    socket.on('start', function(){  //when a "start" message is received from the client, carry out the following:
        timerID = setInterval(function(){ //this function will be repeated for every set interval
          newList = updatestuff(oldList); //update the old list with the new data from Python
          oldList = newList;  //update the old list with the new list
          io.sockets.emit('update', newList); //send the new list to the client
        }, 1000); //interval is 1000ms
    });

    socket.on('stop', function(){ //when a "stop" message is received,
          clearInterval(timerID); //stop the repeated function
    });
});

//Updates the array with new values
function updatestuff (oldItemList)
{
    var currentdate = new Date(); //create a Date object
    var cur_time = timestamp(currentdate);  //call the function timestamp()
    var m_temp, m_roll, m_pitch;  //create variables for the respective maximum values

    //Evaluate maximum values for temperature
    if(loadJSON().temp_data > oldItemList[0]["max_temp"]){
      m_temp = loadJSON().temp_data;  //update maximum value if new data is bigger than previous value
    }

    else{
      m_temp = oldItemList[0]["max_temp"];  //maintain the maximum value
    }

    //Evaluate maximum values for roll
    if(loadJSON().roll_data > oldItemList[0]["max_roll"]){
      m_roll = loadJSON().roll_data;
    }

    else{
      m_roll = oldItemList[0]["max_roll"];
    }

    //Evaluate maximum values for pitch
    if(loadJSON().pitch_data > oldItemList[0]["max_pitch"]){
      m_pitch = loadJSON().pitch_data;
    }

    else{
      m_pitch = oldItemList[0]["max_pitch"];
    }

    //Add new values into the array
    var new_item = {cur_time:cur_time, temp:(loadJSON().temp_data).toFixed(3), roll:(loadJSON().roll_data).toFixed(3), pitch:(loadJSON().pitch_data).toFixed(3), max_temp:m_temp, max_roll:m_roll, max_pitch:m_pitch};
    oldItemList.unshift(new_item);  //add the new values to the start of the array
    oldItemList.pop();  //remove the last element in the array
    return oldItemList;
}

//Create timestamp
function timestamp(date)
{
    //obtain the time from the Date object
    var hours = (date.getHours()).toString();
    var minutes = (date.getMinutes()).toString();
    var seconds = (date.getSeconds()).toString();

    //creates two digits for numbers between 0 to 9
    if(hours.length == 1){
      hours = hours.padStart(2,'0');
    }

    if(minutes.length == 1){
      minutes = minutes.padStart(2,'0');
    }

    if(seconds.length == 1){
      seconds = seconds.padStart(2,'0');
    }

    var time = hours.concat(":",minutes,":",seconds); //combine the strings in the format HH:MM:SS

    return time;
};

//Load JSON file created from Python
function loadJSON()
{
    'use strict';
    const fs = require('fs');
    let rawdata = fs.readFileSync('data.json'); //change the .json file name if necessary
    let values = JSON.parse(rawdata);
    return values;
}
