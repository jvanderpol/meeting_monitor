const ARDUINO_IP_KEY = 'arduino_ip'

async function getArduinoIp() {
  const result = await chrome.storage.local.get([ARDUINO_IP_KEY]);
  if (!result.arduino_ip) {
    throw Error('Unable to load IP from local storage');
  }
  return result.arduino_ip;
}

function saveArduinoIp(ip) {
  return chrome.storage.local.set({
    [ARDUINO_IP_KEY]: ip
  });
}

async function syncMeetings() {
  const token = await chrome.identity.getAuthToken({interactive: true})
  const fetchOptions = {
    method: 'GET',
    async: true,
    headers: {
      Authorization: 'Bearer ' + token.token,
      'Content-Type': 'application/json'
    },
    'contentType': 'json'
  };
  let timeMin = new Date();
  timeMin.setHours(0);
  timeMin.setMinutes(0);
  let timeMax = new Date();
  timeMax.setDate(timeMax.getDate() + 1);
  timeMax.setHours(23);
  timeMax.setMinutes(59);
  const queryString = encode({
    timeMin: timeMin.toISOString(),
    timeMax: timeMax.toISOString(),
    singleEvents: true,
    orderBy: 'startTime'
  });
  const eventsResponse = await fetch(
    `https://www.googleapis.com/calendar/v3/calendars/primary/events?${queryString}`,
    fetchOptions);
  if (eventsResponse && eventsResponse.ok) {
    const events = await eventsResponse.json();
    const acceptedEvents = events.items.filter(accepted);
    const meetingsTimeRanges = toTimeRanges(acceptedEvents, 'default', false);
    const oooTimeRanges = toTimeRanges(acceptedEvents, 'outOfOffice', true);
    const arduinoIp = await getArduinoIp();
    await fetch(`http://${arduinoIp}/setSchedule?meetings=${meetingsTimeRanges}&ooos=${oooTimeRanges}`);
  }
}

function toTimeRanges(events, eventType, includeAllDayEvents) {
  return events
    .filter((e) => e.eventType === eventType)
    .filter((e) => e.start.dateTime || (includeAllDayEvents && e.start.date))
    .map((e) => `${getEpoch(e.start)}-${getEpoch(e.end)}`)
    .join(",");
}

function accepted(calEvent) {
  if (calEvent.organizer.self) {
    return true;
  }
  let myAttendance = calEvent.attendees.find((e) => e.self)
  if (!myAttendance) {
    return false;
  }
  return myAttendance.responseStatus === "accepted";
}

function getEpoch(time) {
  const dateTime = time.dateTime ? time.dateTime : (time.date + "T00:00");
  return new Date(dateTime).getTime() / 1000;
}

function encode(queryParams) {
  return Object.entries(queryParams)
    .map((q) => `${q[0]}=${encodeURIComponent(q[1])}`)
    .join('&');
}
