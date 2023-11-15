importScripts('shared.js')

let last_sent_meeting_status = null;

async function updateMeetingStatus() {
  const tabs = await chrome.tabs.query({url: '*://meet.google.com/*'});
  const meeting_status = tabs.length > 0;
  if (last_sent_meeting_status != meeting_status) {
    last_sent_meeting_status = meeting_status;
    const resource = last_sent_meeting_status ? "meetingStarted" : "meetingEnded";
    await fetch('http://192.168.107.5/' + resource);
  }
}

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  updateMeetingStatus();
});
chrome.tabs.onRemoved.addListener(function(tabId, removeInfo) {
  updateMeetingStatus();
});

setInterval(updateMeetingStatus, 20000);
setInterval(syncMeetings, 10 * 60 * 60000);

chrome.action.onClicked.addListener(function() {
  syncMeetings();
});
