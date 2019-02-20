// Global Options
Chart.defaults.global.defaultFontFamily = 'Lato';
Chart.defaults.global.defaultFontSize = 18;
Chart.defaults.global.defaultFontColor = '#777';

//create arrays to store x-axis and y-axis values for the graphs
temp_xarray = [];
temp_yarray = [];
roll_xarray = [];
roll_yarray = [];
pitch_xarray = [];
pitch_yarray = [];

function makeChart(myChartReplace, x_array, y_array, title_name)
{
  let myChart = document.getElementById(myChartReplace).getContext('2d');
  let massPopChart = new Chart(myChart,
    {
      type:'line', //create a line graph
      data:
      {
        labels:x_array,
        datasets:
        [{
          data:y_array,
          backgroundColor:
          [
            'rgba(0,0,0,0)'
          ],
          borderWidth:3,
          borderColor:'#6c7ae0',
          hoverBorderWidth:10,
          hoverBorderColor:'#000'
        }]
      },
      options:
      {
        title:
        {
          display: true,
          text: title_name
        },
        animation:
        {
          duration:0
        },
        legend:
        {
          display:false,
          position:'right',
          labels:
          {
            fontColor:'#000'
          }
        },
        layout:
        {
          padding:
          {
            left:50,
            right:0,
            bottom:0,
            top:0
          }
        },
        tooltips:
        {
          enabled:true
        }
      }
    });
}

//Initialise the graphs with the respective names
makeChart('tempChart', temp_xarray, temp_yarray, 'Temperature');
makeChart('rollChart', roll_xarray, roll_yarray, 'Roll');
makeChart('pitchChart', pitch_xarray, pitch_yarray, 'Pitch');

socket.on('update', function(data){
  temp_xarray.push(data[0]["cur_time"]);  //extract the values from the list sent from the server
  temp_yarray.push(data[0]["temp"]);      //and put into the respective arrays
  roll_xarray.push(data[0]["cur_time"]);
  roll_yarray.push(data[0]["roll"]);
  pitch_xarray.push(data[0]["cur_time"]);
  pitch_yarray.push(data[0]["pitch"]);
  if(temp_xarray.length == 21)  //set the maximum number of data points shown on the graph
  {
      temp_xarray.shift();  //remove the first element(the oldest value) from the array
      temp_yarray.shift();
      roll_xarray.shift();
      roll_yarray.shift();
      pitch_xarray.shift();
      pitch_yarray.shift();

  }
  makeChart('tempChart', temp_xarray, temp_yarray, 'Temperature');  //update the graphs
  makeChart('rollChart', roll_xarray, roll_yarray, 'Roll');
  makeChart('pitchChart', pitch_xarray, pitch_yarray, 'Pitch');

});
