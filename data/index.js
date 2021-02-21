const fetchSavedData = async () => {
  const  { data } = await fetch('/humidity_data').then(r => r.json()).catch(error => console.error(error))
  const dataArray =data
  .replace('↵', '\n')
  .replace('\r', '\n')
  .split('\n')
  .map(el => ({
    time: new Date(Number(el.split(',')[0])), 
    soilHumidity:Number( el.split(',')[1])
  }))
  .filter(el => !!el.time && (!!el.soilHumidity || el.soilHumidity === 0))
  console.log(dataArray);
  humidityChart.update(parseHumidity(dataArray))
}
setTimeout(fetchSavedData, 500);

const humiditySensorRead = []
const parseHumidity = (humiditySensorReadArray) => ({
  series:[humiditySensorReadArray.map(el => el.soilHumidity)],
  labels: [humiditySensorReadArray.map(el => el.time.toLocaleTimeString())]
})
const humidityChart = new Chartist.Line('#humidityChart', parseHumidity(humiditySensorRead), {
  high: 100,
  low: 0,
  height: '200px',
  // axisX: {
  //   labelInterpolationFnc: function(value) {
  //       const arr = value.split(":")
  //       return  `${arr[0]}:${arr[1]}`
  //   }
  // },
},
  // ['screen and (max-width: 640px)', {
  //   axisX: {
  //     labelInterpolationFnc: function(value) {
  //       const arr = (value[0] || '').split(':')
  //       return `${arr[0]}:${arr[1]}`
  //     }
  //   }
  // }]
// ]
)

const updateSoilHumidityData = async () => {
  try {
    const response = await fetch('/measure_humidity', {
      method: 'post',
      body: {}
    }).then(r => r.json())

    const newValue = response.soilHumidity
    document.querySelector('#current_soil_humidity').innerHTML = `Current soil humidity: ${newValue}`

  } catch (err) {
    console.error(`Error: ${err}`)
  }
}

document.getElementById('btn-measure-humidity').addEventListener('click',updateSoilHumidityData)

document.getElementById('led_on').addEventListener('click', async _ => {
  try {
   await fetch('/led_on', {
      method: 'post',
      body: {}
    })
  } catch (err) {
    console.error(`Error: ${err}`)
  }
})

document.getElementById('led_off').addEventListener('click', async _ => {
  try {
   await fetch('/led_off', {
      method: 'post',
      body: {}
    })
    console.log('Completed!', response.json())
  } catch (err) {
    console.error(`Error: ${err}`)
  }
})

document.getElementById('reset_data').addEventListener('click', async _ => {
  try {
    const proceed = (prompt("This will remove ALL saved data. Type 'ok' to proceed") || '').toLowerCase()
    if(proceed === 'ok'){
      await fetch('/reset_data', {
        method: 'post',
      })
      humidityChart.update(parseHumidity([]))
      setTimeout(fetchSavedData, 500)
    }
  } catch (err) {
    console.error(`Error: ${err}`)
  }
})

document.getElementById('update_data').addEventListener('click', async _ => {
  fetchSavedData()
})
document.getElementById('calibrate_min').addEventListener('click', async _ => {
 await fetch('/calibrate_min')
})
document.getElementById('calibrate_max').addEventListener('click', async _ => {
 await fetch('/calibrate_max')
})

setInterval(fetchSavedData, 1000 * 60 * 10) // update every 10 minutes
