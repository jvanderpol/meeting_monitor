window.onload = async function() {
  const arduino_ip_element = document.getElementById('arduino_ip');
  const status_element = document.getElementById('status');
  status_element.innerHTML = 'Loading...'

  try {
    const ip = await getArduinoIp();
    arduino_ip_element.value = ip;
    status_element.innerHTML = 'IP loaded'
  } catch (e) {
    status_element.innerHTML = `Error loading ip: ${e}`
    console.log(e);
  }
  document.getElementById('save').onclick = async function() {
    try {
      await getArduinoIp()
      await saveArduinoIp(arduino_ip_element.value)
      status_element.innerHTML = 'Saved!'
    } catch (e) {
      status_element.innerHTML = `Error saving ip: ${e}`
      throw e;
    }
  };
  document.getElementById('sync').onclick = async function() {
    status_element.innerHTML = 'Syncing...'
    try {
      await saveArduinoIp(arduino_ip_element.value)
      await syncMeetings()
    } catch (e) {
      status_element.innerHTML = `Error syncing: ${e}`
      console.log(e);
    }
  }
}
