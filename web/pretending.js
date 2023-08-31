
const pretendingPingFunction = (timeFromStart) => {
	return (Math.sin(timeFromStart / (Math.PI * 2 * 3333)) + 1) / 2 * 100;
}
const pretendingRSSIFunction = (timeFromStart) => {
	return (Math.sin(timeFromStart / (Math.PI * 2 * 5000)) + 1) / 2 * -30 - 65;
}
const pretendingTemperatureFunction = (timeFromStart) => {
	return (Math.sin(timeFromStart / (Math.PI * 2 * 8000)) + 1) / 2 * 15 + 15
}

const startTime = +new Date();
const server = new Pretender({ trackRequests: false }, function() {
	this.get('/status', request => {
		const timeFromStart = +new Date() - startTime;
		return [200, {'Content-Type': 'application/json'}, JSON.stringify({
			temperature: pretendingTemperatureFunction(timeFromStart),
			timestamp: new Date().toISOString().slice(0, 19) + 'Z',
			rssi: pretendingRSSIFunction(timeFromStart),
		})]
	}, () => pretendingPingFunction(+new Date() - startTime));
	this.get('/config', request => {
		return [200, {'Content-Type': 'application/json'}, JSON.stringify({
			network: {
				mode: 0,
				ssid: 'PretendingWiFi',
				psk: '12345678',
				static: 1,
				ip: '192.168.55.99',
				mask: 24,
				gateway: '192.168.55.1',
				dns1: '1.1.1.1',
				dns2: '1.0.0.1',
			}
		})];
	}, () => pretendingPingFunction(+new Date() - startTime))
});
