async function applyTilt() {
  const inputMode = document.querySelector('input[name="displayType"]:checked')
  const justifyMode = document.querySelector('input[name="justify"]:checked')
  const ephemeral = document.getElementById('ephemeral').checked
  const ephemeralTime = Number(document.getElementById('ephemeralTime').value)

  var uri = "display";
  var content = document.getElementById('customText').value

  if (inputMode.id == "time") {
    uri += "/date"
    content = document.getElementById('timeText').value
  }

  if (ephemeral) {
    uri += "/ephemeral/" + ephemeralTime
  }

  const response = await fetch(uri, {
    method: 'POST',
    headers: {
      'Accept': 'text/plain',
      'Content-Type': 'text/plain'
    },
    body: content,
    });
}