const button = document.getElementById('btn-measure-humidity')

button.addEventListener('click', async _ => {
  try {
    const response = await fetch('/measure_humidity', {
      method: 'post',
      body: {
        // Your body
      }
    })
    console.log('Completed!', response.json())
  } catch (err) {
    console.error(`Error: ${err}`)
  }
})

new Chartist.Line('#chart1', {
  labels: [1, 2, 3, 4, 6, 7],
  series: [[4, 15, 34, 65, 69, 59, 61]]
})
