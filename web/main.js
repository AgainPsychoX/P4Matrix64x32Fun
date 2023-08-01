const baseHost = document.location.origin;

async function sleep(ms) {
	return new Promise(r => setTimeout(r, ms));
}

function debounce(func, timeout) {
	let handle;
	if (timeout) {
		return function (...args) {
			clearTimeout(handle);
			handle = setTimeout(() => func.apply(this, args), timeout);
		};
	}
	else {
		return function (...args) {
			window.cancelAnimationFrame(handle);
			handle = requestAnimationFrame(() => func.apply(this, args));
		}
	}
}

function parseBoolean(value, defaultValue) {
	if (typeof value === 'undefined') {
		return defaultValue;
	}
	if (typeof value === 'boolean') {
		return value;
	}
	if (typeof value === 'number') {
		return value != 0;
	}
	switch (value.toLowerCase().trim()) {
		case "true": case "yes": case "on": case "1": return true;
		case "false": case "no": case "off": case "0": return false;
		default: return defaultValue;
	}
}

function colorForRSSI(value) {
	/**/ if (value > -65) return '#00A000';
	else if (value > -70) return '#76BA00';
	else if (value > -75) return '#889200';
	else if (value > -80) return '#AA8B00';
	else if (value > -85) return '#AA3C00';
	else if (value > -95) return '#D52800';
	else /*            */ return '#AA1111';
}

function colorForPing(value) {
	/**/ if (value < 50)   return '#00A000';
	else if (value < 100)  return '#76BA00';
	else if (value < 200)  return '#889200';
	else if (value < 333)  return '#AA8B00';
	else if (value < 500)  return '#AA3C00';
	else if (value < 1000) return '#D52800';
	else /*             */ return '#AA1111';
}

function handleFetchResult(promise, successMessage = 'Wysłano!', failureMessage = 'Wystąpił błąd podczas komunikacji z sterownikiem!') {
	return promise
		.then(response => {
			if (response && typeof response.status == 'number' && response.status >= 400) {
				throw new Error(`Serwer zwrócił błąd: HTTP ${response.status}`);
			}
			if (successMessage) {
				alert(successMessage);
			}
			return response;
		})
		.catch(error => {
			if (failureMessage) {
				alert(`${failureMessage}\n\n${error.toString()}`);
			}
			console.error(error);
		})
	;
}

document
	.querySelectorAll('dialog')
	.forEach(dialog => {
		new MutationObserver((list, observer) => {
			for (const mutation of list) {
				if (typeof(mutation.oldValue) == 'object') {
					dialog.classList.add('open');
				}
			}
		}).observe(dialog, { attributeFilter: ['open'], attributeOldValue: true })
		dialog.addEventListener('cancel', e => {
			if (dialog.classList.contains('closing')) return;
			e.preventDefault();
			dialog.classList.add('closing');
		});
		dialog.addEventListener('transitionend', e => {
			if (dialog.classList.contains('closing')) {
				dialog.close();
				dialog.classList.remove('open');
				dialog.classList.remove('closing');
			}
		});
	})
;



////////////////////////////////////////////////////////////////////////////////
// Status

// Status updating
(async () => {
	let lastPing = 0;
	async function updateStatus() {
		const pingOutput = document.querySelector('output[name=ping]');
		const rssiOutput = document.querySelector('output[name=rssi]');
		const start = new Date().getTime();
		let end;
		await fetch(`${baseHost}/status`)
			.then(response => {
				end = new Date().getTime();
				return response.json();
			})
			.then(state => {
				for (const key in state) {
					const value = state[key];
					switch (key) {
						case 'timestamp': {
							const b = value.split(/\D/);
							const d = new Date(b[0], b[1]-1, b[2], b[3], b[4], b[5]);
							document.querySelector('output[name=datetime]').innerText = d.toLocaleString();
							break;
						}
						case 'rssi': {
							rssiOutput.classList.remove('error');
							if (value <= 9) {
								rssiOutput.style.color = colorForRSSI(value);
								rssiOutput.innerText = value + 'dB';
							}
							else {
								rssiOutput.style.color = 'black';
								rssiOutput.innerText = '? (AP)';
							}
							break;
						}
						case 'temperature':
							document.querySelector(`output[name=${key}]`).innerText = value + '°C';
							break;
					}
				}

				lastPing = end - start;
				pingOutput.classList.remove('error');
				pingOutput.style.color = colorForPing(lastPing);
				pingOutput.innerText = `${lastPing}ms`;
			})
			.catch(error => {
				pingOutput.classList.add('error');
				pingOutput.style.color = '';
				pingOutput.innerText = `Brak połączenia!`;
				rssiOutput.classList.add('error');
				rssiOutput.style.color = '';
				rssiOutput.innerText = `?`;
				throw error;
			})
		;
	}
	while (true) {
		try {
			await updateStatus();
			await sleep(1000 - lastPing);
		}
		catch (error) {
			console.error(error);
			await sleep(3333);
		}
	}
})();





////////////////////////////////////////////////////////////////////////////////
// Settings

document.querySelector('button[name=save-eeprom]').addEventListener('click', async () => {
	const promise = Promise.resolve()
		// TODO: save any other parts first
		.then(() => fetch(`${baseHost}/saveEEPROM`))
	;
	handleFetchResult(promise);
});

// Network settings
const networkSettingsDialog = document.querySelector('dialog#network-settings');
{
	const fields = ['ssid', 'psk', 'ip', 'mask', 'gateway', 'dns1', 'dns2'];

	const openButton = document.querySelector('button[name=open-network-settings]');
	openButton.addEventListener('click', () => {
		const promise = fetch(`${baseHost}/config`)
			.then(response => response.json())
			.then(state => {
				for (const name of fields) {
					networkSettingsDialog.querySelector(`input[name=${name}]`).value = state.network[name];
				}
				switch (state.network.mode) {
					case 0:
				}
				networkSettingsDialog.querySelector(`input[name=mode][value=${state.network.mode & 1 ? 'sta' : 'ap'}]`).checked = true;
				networkSettingsDialog.querySelector(`input[name=fallback-ap]`).checked = parseBoolean(state.network.mode > 1);
				networkSettingsDialog.querySelector(`input[name=auto-ip]`).checked = !parseBoolean(state.network.static);
				networkSettingsDialog.showModal();
			})
		;
		handleFetchResult(promise, '');
	});

	const form = networkSettingsDialog.querySelector('form');

	networkSettingsDialog.querySelector('button[name=save]').addEventListener('click', async (e) => {
		if (!form.checkValidity()) return;

		if (!confirm("Jesteś pewien, że chcesz zapisać zamiany? Urządzenie zostanie zresetowane.")) return;

		e.preventDefault();
		networkSettingsDialog.classList.add('closing');

		const mode = (networkSettingsDialog.querySelector(`input[name=fallback-ap]`).checked ? 2 : 0) | (document.querySelector(`input[name=mode]:checked`).value == 'ap' ? 0 : 1);
		const querystring = (
			fields
				.map(name => `network.${name}=${encodeURIComponent(networkSettingsDialog.querySelector(`input[name=${name}]`).value)}`)
				.join('&') +
			`&network.static=${networkSettingsDialog.querySelector(`input[name=auto-ip]`).checked ? 0 : 1}` +
			`&network.mode=${mode}`
		);
		await handleFetchResult(fetch(`${baseHost}/config?${querystring}`), 'Wysłano! Urządzenie zostanie zresetowane...');
		window.close();
	});
	networkSettingsDialog.querySelector('button[name=cancel]').addEventListener('click', async (e) => {
		e.preventDefault();
		networkSettingsDialog.classList.add('closing');
	});
}

// Load settings
fetch(`${baseHost}/config`)
	.then(response => response.json())
	.then(state => {
		// TODO: display config to the page
	})
	.catch((error) => {
		alert(`Błąd połączenia! Nie można załadować ustawień. Odśwież stronę...`);
		console.error(error);
	})
;





