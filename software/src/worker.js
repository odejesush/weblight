let devices = new Array();

navigator.usb.addEventListener('connect', (event) => {
  devices.push(event.device);
  connectDevice(event.device);
});

navigator.usb.addEventListener('disconnect', (event) => {
  let index = devices.indexOf(event.device);
  if (index < 0) return;
  devices.splice(index, 1);
  postMessage({type: 'disconnect', serialNumber: event.device.serialNumber});
});

function getDeviceIndex(serialNumber) {
  for (let i = 0; i < devices.length; ++i) {
    if (serialNumber === devices[i].serialNumber) return i;
  }
}

async function updateDeviceList(serialNumber) {
  let connectedDevices = await navigator.usb.getDevices();
  for (let device of connectedDevices) {
    if (serialNumber === device.serialNumber) {
      devices.push(device);
      connectDevice(device);
      return;
    }
  }
}

async function connectDevice(device) {
  await device.open();
  if (device.configuration === null) await device.selectConfiguration(1);
  await device.claimInterface(0);
  postMessage({type: 'device opened', serialNumber: device.serialNumber});
};

async function setDeviceColor(serialNumber, r, g, b) {
  let device = devices[getDeviceIndex(serialNumber)];

  if (device.opened) {
    let payload;

    if (device.usbVersionMajor === 2) {
      payload = new Uint8Array([r, g, b]);
    } else if (device.usbVersionMajor === 1) {
      payload = new Uint8Array([0xFF, r, g, b])
    }

    if (payload == null) {
      throw new Error(
          `Unknown device firmware version ${device.usbVersionMajor}`);
    }

    await device.controlTransferOut(
        {
          requestType: 'vendor',
          recipient: 'device',
          request: 1,
          value: 0,
          index: 0,
        },
        payload);
  }
}

onmessage = async (event) => {
  switch (event.data.type) {
    case 'connectDevice':
      await updateDeviceList(event.data.serialNumber);
      break;
    case 'connectDevices':
      devices = await navigator.usb.getDevices()
      devices.forEach(async (device) => connectDevice(device));
      break;
    case 'setDeviceColor':
      await setDeviceColor(
          event.data.serialNumber, event.data.r, event.data.g, event.data.b);
      break;
  }
}
