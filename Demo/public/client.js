//Make connection
var socket = io.connect('http://localhost:5000');

//Query DOM
var btn = document.getElementById('send');
var date = document.getElementById('date');
var max = document.getElementById('max');
var readings = document.getElementById('readings');

//Emit events
var click = true; //taggle for start/stop button
btn.innerHTML = "Start";

btn.addEventListener('click', function(){ //when the button is clicked, carry out the following:
  if(click){
      socket.emit('start'); //send a "start" message to server
      btn.innerHTML = "Stop";
  }
  else{
    socket.emit('stop');  //send a "stop" message to server
    btn.innerHTML = "Start";
  }
  click = !click; //toggle the Boolean variable
});

//Listen for the "update" message from server
socket.on('update', function(data){
  //create a dynamic table on the webpage
  readings.innerHTML = '<p>' + data[0]["cur_time"]+'<attr1>' + data[0]["temp"] + '</attr1><attr2>'+ data[0]["roll"] + '</attr2><attr3>'+ data[0]["pitch"] + '</attr3> </p>' + readings.innerHTML;
  //prints out the maximum values on the webpage
  max.innerHTML = '<p>'+"Max Temperature: " + data[0]["max_temp"]+'°C'+ '<attr1>'+"Max Roll: " + data[0]["max_roll"] +'°'+ '</attr1><attr2>'+"Max Pitch: "+ data[0]["max_pitch"]+'°'+'</attr2></p>';
});
